#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "ksocket.h"

#define FILE_TO_SAVE "output.txt"
#define CHUNK_SIZE 512

extern int my_errno;

int main()
{
    printf("--- User 2 (Receiver) Starting ---\n");

    // 1. Create the KTP socket
    int sock_fd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock_fd < 0)
    {
        printf("Error: Failed to create KTP socket.\n");
        return 1;
    }

    // 2. Setup IP and Ports (Swapped from user1)
    struct sockaddr_in src_addr, dest_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(8081); // user2 local port
    inet_pton(AF_INET, "127.0.0.1", &src_addr.sin_addr);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8080); // user1 remote port
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    // 3. Bind the socket
    if (k_bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr),
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
    {
        printf("Error: Failed to bind socket.\n");
        return 1;
    }

    // 4. Open the file to write
    // Creates the file if it doesn't exist, truncates it if it does
    int fd = open(FILE_TO_SAVE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        perror("Error opening output file");
        return 1;
    }

    // 5. Receive the file in chunks
    char buffer[CHUNK_SIZE + 1]; // +1 for null termination if needed
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    int total_received = 0;

    printf("Waiting for file transfer...\n");

    while (1)
    {
        int bytes_received = k_recvfrom(sock_fd, buffer, CHUNK_SIZE, 0,
                                        (struct sockaddr *)&sender_addr, &sender_len);

        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0'; // Null terminate to check for EOF safely

            // Check if it's the end of the file
            if (strcmp(buffer, "<EOF>") == 0)
            {
                printf("Received EOF marker. Transfer complete.\n");
                break;
            }

            // Write the chunk to the file
            write(fd, buffer, bytes_received);
            total_received += bytes_received;
        }
        else if (bytes_received < 0)
        {
            if (my_errno == ENOMESSAGE)
            {
                // No message available right now. Wait a bit before polling again.
                usleep(10000); // Wait 10ms
            }
            else
            {
                printf("Unexpected error during receive.\n");
                break;
            }
        }
    }

    printf("Total bytes received: %d\n", total_received);

    close(fd);
    k_close(sock_fd);
    return 0;
}