/*
=====================================================
Mini Project 1 Submission
Group Details :
Member 1 Name : Dake Yuvan Gates
Member 1 Roll number : 23CS10015
Member 2 Name : Marala Sai Pragnaan
Member 2 Roll number : 23CS10043
=====================================================
*/

#include "ksocket.h"
#include <string.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <signal.h>

extern Shared_Mem *SM;

void *R_handler(void *arg)
{
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;

    while (1)
    {
        timeout.tv_sec = T;
        timeout.tv_usec = 0;
        FD_ZERO(&read_fds);
        max_fd = 0;

        // Create and bind underlying UDP sockets if user called k_bind()
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (!SM[i].isFree && SM[i].udp_sock_fd == -1 && SM[i].local_port != 0)
            {
                pthread_mutex_lock(&SM[i].mutex);
                int new_fd = socket(AF_INET, SOCK_DGRAM, 0);
                if (new_fd >= 0)
                {
                    struct sockaddr_in local_addr;
                    memset(&local_addr, 0, sizeof(local_addr));
                    local_addr.sin_family = AF_INET;
                    local_addr.sin_addr.s_addr = INADDR_ANY;
                    local_addr.sin_port = htons(SM[i].local_port);

                    if (bind(new_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) == 0)
                    {
                        SM[i].udp_sock_fd = new_fd;
                        printf("Thread R: Bound KTP ID %d to UDP fd %d on port %d\n", i, new_fd, SM[i].local_port);
                    }
                    else
                    {
                        perror("Thread R: Bind failed");
                        close(new_fd);
                    }
                }
                pthread_mutex_unlock(&SM[i].mutex);
            }
        }

        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            int current_fd = SM[i].udp_sock_fd;
            if (current_fd >= 0)
            {
                FD_SET(current_fd, &read_fds);
                if (current_fd > max_fd)
                    max_fd = current_fd;
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            perror("select error");
            break;
        }

        // Handle Timeout & nospace flag [cite: 91, 92]
        if (activity == 0)
        {
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                if (!SM[i].isFree && SM[i].nospace)
                {
                    pthread_mutex_lock(&SM[i].mutex);
                    int free_space = 10 - SM[i].total_messages_in_buffer;
                    if (free_space > 0)
                    {
                        struct sockaddr_in dest_addr;
                        memset(&dest_addr, 0, sizeof(dest_addr));
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].des_port);
                        inet_pton(AF_INET, SM[i].des_ip, &dest_addr.sin_addr);

                        ktp_header ack_pkt;
                        ack_pkt.type = 'A';
                        ack_pkt.seq_num = SM[i].rwnd.expected_seq - 1;
                        ack_pkt.window_size = free_space;

                        sendto(SM[i].udp_sock_fd, &ack_pkt, sizeof(ktp_header), 0,
                               (struct sockaddr *)&dest_addr, sizeof(dest_addr));

                        printf("Thread R: Sent duplicate ACK (Seq %d) due to freed space. Resetting nospace.\n", ack_pkt.seq_num);
                        SM[i].nospace = false;
                    }
                    pthread_mutex_unlock(&SM[i].mutex);
                }
            }
            continue;
        }

        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            int current_fd = SM[i].udp_sock_fd;
            if (current_fd < 0 || SM[i].isFree)
                continue;

            if (FD_ISSET(current_fd, &read_fds))
            {
                char packet_buffer[MAX_PACKET_SIZE];
                struct sockaddr_in sender_addr;
                socklen_t sender_len = sizeof(sender_addr);

                int bytes_received = recvfrom(current_fd, packet_buffer, MAX_PACKET_SIZE, 0,
                                              (struct sockaddr *)&sender_addr, &sender_len);

                if (bytes_received > 0)
                {
                    if (dropMessage(DROP_PROB))
                    {
                        continue; // Simulated loss
                    }

                    ktp_header *header = (ktp_header *)packet_buffer;
                    char *payload = packet_buffer + sizeof(ktp_header);
                    int payload_len = bytes_received - sizeof(ktp_header);

                    pthread_mutex_lock(&SM[i].mutex);

                    if (header->type == 'D')
                    {
                        uint8_t expected = SM[i].rwnd.expected_seq;
                        uint8_t offset = header->seq_num - expected; // 8-bit math handles wraparound!
                        int free_space = 10 - SM[i].total_messages_in_buffer;

                        if (offset < free_space)
                        { // Message is within window
                            int write_index = (SM[i].recv_head + offset) % 10;

                            // Buffer it if we don't already have it
                            if (!SM[i].recv_valid[write_index])
                            {
                                memset(SM[i].recv_buffer[write_index], 0, MAX_PAYLOAD_SIZE);
                                memcpy(SM[i].recv_buffer[write_index], payload, payload_len);
                                SM[i].recv_valid[write_index] = true;
                                SM[i].total_messages_in_buffer++;

                                if (offset == 0)
                                {
                                    // It's the expected IN-ORDER message
                                    // Slide window past any contiguous buffered messages
                                    while (SM[i].recv_valid[SM[i].recv_head] && SM[i].recv_count < 10)
                                    {
                                        SM[i].rwnd.expected_seq++;
                                        SM[i].recv_head = (SM[i].recv_head + 1) % 10;
                                        SM[i].recv_count++;
                                    }

                                    free_space = 10 - SM[i].total_messages_in_buffer;
                                    if (free_space == 0)
                                        SM[i].nospace = true;

                                    // Send ACK for the highest contiguous sequence received [cite: 89]
                                    ktp_header ack_pkt;
                                    ack_pkt.type = 'A';
                                    ack_pkt.seq_num = SM[i].rwnd.expected_seq - 1;
                                    ack_pkt.window_size = free_space;

                                    sendto(current_fd, &ack_pkt, sizeof(ktp_header), 0,
                                           (struct sockaddr *)&sender_addr, sender_len);
                                }
                                else
                                {
                                    // Out of order [cite: 43]
                                    printf("Thread R: Buffered OUT-OF-ORDER Seq %d (Expected %d). No ACK sent.\n", header->seq_num, expected);
                                }
                            }
                        }
                        else if (offset >= free_space && offset > 128)
                        {
                            // High offset means sequence is OLD/DUPLICATE (wrapped back around) [cite: 44]
                            ktp_header ack_pkt;
                            ack_pkt.type = 'A';
                            ack_pkt.seq_num = expected - 1;
                            ack_pkt.window_size = free_space;

                            sendto(current_fd, &ack_pkt, sizeof(ktp_header), 0,
                                   (struct sockaddr *)&sender_addr, sender_len);
                        }
                    }
                    else if (header->type == 'A')
                    {
                        uint8_t unack = SM[i].swnd.unack_seq;
                        uint8_t offset = header->seq_num - unack; // Distance from unack base

                        if (offset < SM[i].send_count)
                        {
                            // NEW ACK: It acknowledges (offset + 1) messages
                            int acked_messages = offset + 1;
                            SM[i].send_count -= acked_messages;
                            if (SM[i].send_count < 0)
                                SM[i].send_count = 0;

                            SM[i].swnd.unack_seq = header->seq_num + 1;
                            SM[i].swnd.window_size = header->window_size;
                        }
                        else
                        {
                            // DUPLICATE ACK: Just update the window size [cite: 94]
                            SM[i].swnd.window_size = header->window_size;
                        }
                    }

                    pthread_mutex_unlock(&SM[i].mutex);
                }
            }
        }
    }
    return NULL;
}

void *S_handler(void *arg)
{
    while (1)
    {
        usleep((T * 1000000) / 2); // Sleep for T/2 seconds
        time_t current_time = time(NULL);

        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (SM[i].udp_sock_fd < 0 || SM[i].isFree)
                continue;

            pthread_mutex_lock(&SM[i].mutex);

            // 1. Check for Timeout
            uint8_t inflight = SM[i].swnd.next_seq_to_send - SM[i].swnd.unack_seq;
            if (inflight > 0 && (current_time - SM[i].last_msg_time >= T))
            {
                // Timeout! Reset next_seq_to_send to force retransmission [cite: 96, 97]
                SM[i].swnd.next_seq_to_send = SM[i].swnd.unack_seq;
            }

            // 2. Transmit Pending Messages [cite: 98]
            int oldest_index = (SM[i].send_head - SM[i].send_count + 10) % 10;
            uint8_t offset = SM[i].swnd.next_seq_to_send - SM[i].swnd.unack_seq;

            while (offset < SM[i].send_count && offset < SM[i].swnd.window_size)
            {
                int buffer_index = (oldest_index + offset) % 10;

                ktp_header pkt;
                pkt.type = 'D';
                pkt.seq_num = SM[i].swnd.next_seq_to_send;
                pkt.window_size = 0;

                char packet_buffer[MAX_PACKET_SIZE];
                memcpy(packet_buffer, &pkt, sizeof(ktp_header));
                memcpy(packet_buffer + sizeof(ktp_header), SM[i].send_buffer[buffer_index], MAX_PAYLOAD_SIZE);

                struct sockaddr_in dest_addr;
                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(SM[i].des_port);
                inet_pton(AF_INET, SM[i].des_ip, &dest_addr.sin_addr);

                sendto(SM[i].udp_sock_fd, packet_buffer, MAX_PACKET_SIZE, 0,
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr));

                SM[i].last_msg_time = current_time;
                SM[i].swnd.next_seq_to_send++;
                offset = SM[i].swnd.next_seq_to_send - SM[i].swnd.unack_seq;
            }

            pthread_mutex_unlock(&SM[i].mutex);
        }
    }
    return NULL;
}

void *GC_handler(void *arg)
{
    while (1)
    {
        sleep(10);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (!SM[i].isFree && SM[i].pid > 0)
            {
                if (kill(SM[i].pid, 0) == -1)
                {
                    pthread_mutex_lock(&SM[i].mutex);
                    close(SM[i].udp_sock_fd);
                    SM[i].isFree = true;
                    SM[i].udp_sock_fd = -1;
                    pthread_mutex_unlock(&SM[i].mutex);
                }
            }
        }
    }
    return NULL;
}

int main()
{
    pthread_t R, S, GC;

    key_t key = ftok(FTOK_FILE, FTOK_PROJ_ID);
    int shmid = shmget(key, sizeof(Shared_Mem) * MAX_SOCKETS, IPC_CREAT | 0666);
    SM = (Shared_Mem *)shmat(shmid, NULL, 0);

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].isFree = true;
        SM[i].udp_sock_fd = -1;
        pthread_mutex_init(&SM[i].mutex, &mutex_attr);
    }
    pthread_mutexattr_destroy(&mutex_attr);

    pthread_create(&R, NULL, R_handler, NULL);
    pthread_create(&S, NULL, S_handler, NULL);
    pthread_create(&GC, NULL, GC_handler, NULL);

    pthread_join(R, NULL);
    pthread_join(S, NULL);
    pthread_join(GC, NULL);

    return 0;
}