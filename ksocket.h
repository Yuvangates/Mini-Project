#ifndef KSOCKET_H
#define KSOCKET_H

#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ipc.h>

#define T 5
#define DROP_PROB 0.15
#define MAX_SOCKETS 100
#define SOCK_KTP 0x1234
#define ENOTBOUND 1000
#define ENOSPACE 1001
#define ENOMESSAGE 1002
#define MAX_PAYLOAD_SIZE 512

#define FTOK_FILE "ksocket.h"
#define FTOK_PROJ_ID 'K'
#define MAX_PACKET_SIZE (sizeof(ktp_header) + MAX_PAYLOAD_SIZE)

extern int my_errno;

typedef struct
{
    char type;
    uint8_t seq_num;
    int window_size;
    uint16_t payload_len;
} ktp_header;

struct swnd
{
    uint8_t unack_seq;
    uint8_t next_seq_to_send;
    int window_size;
} typedef sender_wnd;

struct rwnd
{
    uint8_t expected_seq;
    int window_size;
} typedef reciever_wnd;

typedef struct
{
    bool isFree;
    pid_t pid;
    int udp_sock_fd;
    char des_ip[INET_ADDRSTRLEN];
    int des_port;
    uint16_t local_port;

    char send_buffer[10][MAX_PAYLOAD_SIZE];
    int send_len[10];
    char recv_buffer[10][MAX_PAYLOAD_SIZE];
    int recv_len[10];

    int send_head;
    int send_count;

    int user_read_head;
    int recv_head;
    int recv_count;
    int total_messages_in_buffer;
    bool recv_valid[10];
    bool nospace;

    int total_app_messages;
    int total_udp_transmissions;

    sender_wnd swnd;
    reciever_wnd rwnd;

    time_t last_msg_time;

    pthread_mutex_t mutex;
} Shared_Mem;

int k_socket(int family, int type, int protocol);
int k_bind(int sockfd, const struct sockaddr *src_addr, socklen_t src_len, const struct sockaddr *dest_addr, socklen_t dest_len);
int k_sendto(int sock_fd, const void *message, size_t message_len, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
int k_recvfrom(int sock_fd, void *buffer, size_t buffer_len, int flags, struct sockaddr *src_addr, socklen_t *src_len);
int k_close(int sock_fd);
int dropMessage(float p);

#endif