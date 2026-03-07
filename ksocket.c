#include "ksocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/*
k_socket – This function opens an UDP socket with the socket call. The parameters
to these are the same as the normal socket() call, except that it will take only
SOCK_KTP as the socket type. k_socket() checks whether a free space is
available in the SM, creates the corresponding UDP socket if a free space is
available, and initializes SM with corresponding entries. If no free space is available,
it returns -1 with the global error variable set to ENOSPACE.
*/

int k_socket(int family, int type, int protocol)
{
    if (type != SOCK_KTP)
    {
        return -1;
    }
    // check for the free space in the SM
    // -1 if no free space is available
    int sock_fd = socket(family, SOCK_DGRAM, protocol);
    if (sock_fd < 0)
    {
        perror("udp socket creation failed");
        return -1;
    }
    printf("socket created with fd : %d\n", sock_fd);
    return sock_fd;
}

/*
k_bind – binds the socket with some address-port using the bind call. Bind is
necessary for each KTP socket irrespective of whether it is used as a server or a
client. This function takes the source IP, the source port, the destination IP and the
destination port. It binds the UDP socket with the source IP and source port, and
updates the corresponding SM with the destination IP and destination port.
*/

int k_bind(int sockfd, const struct sockaddr *src_addr, socklen_t src_len, const struct sockaddr *dest_addr, socklen_t dest_len)
{
    if (bind(sockfd, src_addr, src_len) < 0)
    {
        perror("bind failed");
        return -1;
    }

    struct sockaddr_in *src_addr_in = (struct sockaddr_in *)src_addr;
    // update the corresponding SM with the destination IP and destination port
    printf("socket with fd :%d binded successfully to port :%d\n", sockfd, ntohs(src_addr_in->sin_port));
    return 0;
}

// where should i implement SM? in this same file or separate file? give answer?

/*
k_sendto – writes the message to the sender side message buffer if the destination
        IP /
    Port matches with the bounded IP / Port as set through k_bind().If not,
    it drops
    the message,
    returns - 1 and sets the global error variable to ENOTBOUND.If there
              is no space is the send buffer,
    return -1 and set the global error variable to
            ENOSPACE.Note that you need to define these error variables properly in a header
            file.
*/

int k_sendto(int sock_fd, const void *message, size_t message_len, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
    // check dest ip / port bounded ip/port
    if (dropMessage(DROP_PROB))
    {
        printf("Message dropped due to network conditions\n");
        return -1;
    }
    ssize_t sent_bytes = sendto(sock_fd, message, message_len, flags, dest_addr, dest_len);
    if (sent_bytes < 0)
    {
        perror("sendto failed");
        return -1;
    }
}
int dropMessage(float p)
{
    float r = (float)rand() / RAND_MAX;
    if (r < p)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}