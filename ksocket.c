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
#include <sys/shm.h>
#include <arpa/inet.h>
#include <string.h>

Shared_Mem *SM = NULL;
int my_errno = 0;

int attach_shared_memory()
{
    if (SM != NULL)
        return 0;
    key_t key = ftok(FTOK_FILE, FTOK_PROJ_ID);
    int shmid = shmget(key, sizeof(Shared_Mem) * MAX_SOCKETS, 0666);
    SM = (Shared_Mem *)shmat(shmid, NULL, 0);
    if (SM == (void *)-1)
        return -1;
    return 0;
}

int k_socket(int family, int type, int protocol)
{
    if (type != SOCK_KTP)
        return -1;
    if (attach_shared_memory() < 0)
        return -1;

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        pthread_mutex_lock(&SM[i].mutex);
        if (SM[i].isFree)
        {
            SM[i].isFree = false;
            SM[i].pid = getpid();
            SM[i].udp_sock_fd = -1;
            SM[i].local_port = 0;

            SM[i].send_count = 0;
            SM[i].send_head = 0;

            SM[i].user_read_head = 0;
            SM[i].recv_head = 0;
            SM[i].recv_count = 0;
            SM[i].total_messages_in_buffer = 0;
            SM[i].nospace = false;

            for (int j = 0; j < 10; j++)
                SM[i].recv_valid[j] = false;

            // Seq numbers start at 1 [cite: 38]
            SM[i].swnd.unack_seq = 1;
            SM[i].swnd.next_seq_to_send = 1;
            SM[i].swnd.window_size = 10;

            SM[i].rwnd.expected_seq = 1;
            SM[i].rwnd.window_size = 10;

            SM[i].last_msg_time = 0;

            pthread_mutex_unlock(&SM[i].mutex);
            return i;
        }
        pthread_mutex_unlock(&SM[i].mutex);
    }
    my_errno = ENOSPACE;
    return -1;
}

int k_bind(int sockfd, const struct sockaddr *src_addr, socklen_t src_len, const struct sockaddr *dest_addr, socklen_t dest_len)
{
    if (attach_shared_memory() < 0 || sockfd < 0 || sockfd >= MAX_SOCKETS)
        return -1;

    struct sockaddr_in *dest_addr_in = (struct sockaddr_in *)dest_addr;
    struct sockaddr_in *src_addr_in = (struct sockaddr_in *)src_addr;

    pthread_mutex_lock(&SM[sockfd].mutex);
    SM[sockfd].local_port = ntohs(src_addr_in->sin_port);
    inet_ntop(AF_INET, &(dest_addr_in->sin_addr), SM[sockfd].des_ip, INET_ADDRSTRLEN);
    SM[sockfd].des_port = ntohs(dest_addr_in->sin_port);
    pthread_mutex_unlock(&SM[sockfd].mutex);

    return 0;
}

int k_sendto(int sock_fd, const void *message, size_t message_len, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
    if (attach_shared_memory() < 0 || sock_fd < 0 || sock_fd >= MAX_SOCKETS)
        return -1;

    struct sockaddr_in *dest_addr_in = (struct sockaddr_in *)dest_addr;
    char req_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(dest_addr_in->sin_addr), req_ip, INET_ADDRSTRLEN);
    int req_port = ntohs(dest_addr_in->sin_port);

    pthread_mutex_lock(&SM[sock_fd].mutex);
    if (strcmp(req_ip, SM[sock_fd].des_ip) != 0 || req_port != SM[sock_fd].des_port)
    {
        my_errno = ENOTBOUND;
        pthread_mutex_unlock(&SM[sock_fd].mutex);
        return -1;
    }

    if (SM[sock_fd].send_count >= 10)
    {
        my_errno = ENOSPACE;
        pthread_mutex_unlock(&SM[sock_fd].mutex);
        return -1;
    }

    int write_index = SM[sock_fd].send_head;
    size_t copy_len = (message_len > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE : message_len;

    memset(SM[sock_fd].send_buffer[write_index], 0, MAX_PAYLOAD_SIZE);
    memcpy(SM[sock_fd].send_buffer[write_index], message, copy_len);

    SM[sock_fd].send_head = (write_index + 1) % 10;
    SM[sock_fd].send_count++;

    pthread_mutex_unlock(&SM[sock_fd].mutex);
    return copy_len;
}

int k_recvfrom(int sock_fd, void *buffer, size_t buffer_len, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
    if (attach_shared_memory() < 0 || sock_fd < 0 || sock_fd >= MAX_SOCKETS)
        return -1;

    pthread_mutex_lock(&SM[sock_fd].mutex);

    if (SM[sock_fd].recv_count <= 0)
    {
        my_errno = ENOMESSAGE;
        pthread_mutex_unlock(&SM[sock_fd].mutex);
        return -1;
    }

    int read_index = SM[sock_fd].user_read_head;
    size_t copy_len = (buffer_len > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE : buffer_len;
    memcpy(buffer, SM[sock_fd].recv_buffer[read_index], copy_len);

    // Clear out the slot
    SM[sock_fd].recv_valid[read_index] = false;
    memset(SM[sock_fd].recv_buffer[read_index], 0, MAX_PAYLOAD_SIZE);

    SM[sock_fd].user_read_head = (read_index + 1) % 10;
    SM[sock_fd].recv_count--;
    SM[sock_fd].total_messages_in_buffer--;

    // Populate the source details for the user application
    if (src_addr != NULL && src_len != NULL)
    {
        struct sockaddr_in *src_addr_in = (struct sockaddr_in *)src_addr;
        src_addr_in->sin_family = AF_INET;
        src_addr_in->sin_port = htons(SM[sock_fd].des_port);
        inet_pton(AF_INET, SM[sock_fd].des_ip, &(src_addr_in->sin_addr));
        *src_len = sizeof(struct sockaddr_in);
    }

    pthread_mutex_unlock(&SM[sock_fd].mutex);
    return copy_len;
}

int k_close(int sock_fd)
{
    if (attach_shared_memory() < 0 || sock_fd < 0 || sock_fd >= MAX_SOCKETS)
        return -1;

    // FIX: Graceful Shutdown. Wait for all buffered messages to be ACKed!
    while (1)
    {
        pthread_mutex_lock(&SM[sock_fd].mutex);
        if (SM[sock_fd].send_count == 0)
        {
            pthread_mutex_unlock(&SM[sock_fd].mutex);
            break;
        }
        pthread_mutex_unlock(&SM[sock_fd].mutex);
        usleep(50000); // Wait 50ms before checking again
    }

    // Now safely tear down the socket
    pthread_mutex_lock(&SM[sock_fd].mutex);

    if (SM[sock_fd].udp_sock_fd >= 0)
    {
        close(SM[sock_fd].udp_sock_fd);
        SM[sock_fd].udp_sock_fd = -1;
    }

    SM[sock_fd].pid = 0;
    memset(SM[sock_fd].des_ip, 0, INET_ADDRSTRLEN);
    SM[sock_fd].des_port = 0;
    SM[sock_fd].isFree = true;

    pthread_mutex_unlock(&SM[sock_fd].mutex);
    return 0;
}

int dropMessage(float p)
{
    float r = (float)rand() / RAND_MAX;
    return (r < p) ? 1 : 0;
}