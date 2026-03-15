/*
== == == == == == == == == == == == == == == == == ==
Mini Project 1 Submission
Group Details :
Member 1 Name : Dake Yuvan Gates
Member 1 Roll number : 23CS10015
Member 2 Name : Marala Sai Pragnaan
Member 2 Roll number : 23CS10043
== == == == == == == == == == == == == == == == == ==
*/

#include "ksocket.h"

Shared_Mem *SM;

void *R_handler(void *arg)
{
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;
    while (1)
    {
        // 1. Clear the set completely
        timeout.tv_sec = T;
        timeout.tv_usec = 0;
        FD_ZERO(&read_fds);
        max_fd = 0;

        // 2. Add all N sockets to the set and find the highest FD
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            int current_fd = SM[i].udp_sock_fd;
            if (current_fd < 0)
            {
                continue;
            }

            // Add this specific socket to the monitoring set
            FD_SET(current_fd, &read_fds);

            // Keep track of the highest file descriptor number
            if (current_fd > max_fd)
            {
                max_fd = current_fd;
            }
        }

        // 3. Call select()
        // We pass max_fd + 1. The last parameter is the timeout (NULL means wait forever).
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            perror("select error");
            break;
        }

        // 4. Find out WHICH socket has data waiting
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            int current_fd = SM[i].udp_sock_fd;

            // FD_ISSET returns true if 'current_fd' is the one that woke up select()
            if (FD_ISSET(current_fd, &read_fds))
            {

                printf("Packet received on socket descriptor: %d\n", current_fd);

                // Now it is safe to call recvfrom() without blocking!
                char buffer[1024];
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int bytes = recvfrom(current_fd, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr *)&client_addr, &client_len);

                if (bytes > 0)
                {
                    buffer[bytes] = '\0'; // Null-terminate if it's a string
                    // store the message in the recv_buffer
                    // send an ack to the sender
                    // buffer is full means
                    SM[i].isFree = false;
                    printf("Data: %s\n", buffer);
                }
            }
        }
    }
}

void *S_handler(void *arg)
{
}

int main()
{
    pthread_t R, S;

    key_t key = ftok(FTOK_FILE, FTOK_PROJ_ID);
    if (key == -1)
    {
        perror("initksocket: ftok failed");
        exit(1);
    }

    int shmid = shmget(key, sizeof(Shared_Mem) * MAX_SOCKETS, IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("initksocket: shmget failed");
        exit(1);
    }
    SM = (Shared_Mem *)shmat(shmid, NULL, 0);
    if (SM == (void *)-1)
    {
        perror("initksocket: shmat failed");
        exit(1);
    }
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);

    // THIS IS THE MAGIC LINE that allows cross-process locking
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].isFree = true;
        SM[i].udp_sock_fd = -1;
        if (pthread_mutex_init(&SM[i].mutex, &mutex_attr) != 0)
        {
            perror("Mutex init failed");
            exit(1);
        }
    }
    pthread_mutexattr_destroy(&mutex_attr);

    pthread_create(&R, NULL, R_handler, NULL);
    pthread_create(&S, NULL, S_handler, NULL);

    pthread_join(R, NULL);
    pthread_join(S, NULL);

    return 0;
}
