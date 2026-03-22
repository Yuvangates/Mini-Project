#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "ksocket.h"

#define FILE_TO_SEND "input.txt"
#define CHUNK_SIZE 512

extern int my_errno;

int main()
{
    printf("--- User 1 (Sender) Starting ---\n");

    int sock_fd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock_fd < 0)
    {
        printf("Error: Failed to create KTP socket.\n");
        return 1;
    }

    struct sockaddr_in src_addr, dest_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &src_addr.sin_addr);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8081);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    if (k_bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr),
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
    {
        printf("Error: Failed to bind socket.\n");
        return 1;
    }

    int fd = open(FILE_TO_SEND, O_RDONLY);
    if (fd < 0)
    {
        perror("Error opening input file");
        return 1;
    }

    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;
    int total_sent = 0;

    printf("Starting file transfer...\n");
    while ((bytes_read = read(fd, buffer, CHUNK_SIZE)) > 0)
    {
        int sent = -1;

        while (sent < 0)
        {
            sent = k_sendto(sock_fd, buffer, bytes_read, 0,
                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));

            if (sent < 0)
            {
                if (my_errno == ENOSPACE)
                {
                    usleep(10000);
                }
                else if (my_errno == ENOTBOUND)
                {
                    printf("Error: Destination not bound correctly.\n");
                    exit(1);
                }
            }
        }
        total_sent += sent;
    }
    char eof_marker[] = "<EOF>";
    while (k_sendto(sock_fd, eof_marker, strlen(eof_marker), 0,
                    (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
    {
        usleep(10000);
    }

    printf("File transfer complete. Total bytes sent: %d\n", total_sent);

    close(fd);
    k_close(sock_fd);
    return 0;
}