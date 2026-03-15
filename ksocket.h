#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/types.h> // Required for the key_t data type
#include <sys/ipc.h>

#define T 5
#define DROP_PROB 0.5
#define MAX_SOCKETS 100
#define SOCK_KTP 0x1234
#define ENOTBOUND 1000
#define ENOSPACE 1001
#define ENOMESSAGE 1002

#define FTOK_FILE "ksocket.h" // This file must exist in the directory you run the code from
#define FTOK_PROJ_ID 'K'

struct swnd
{
    int seq_num[256];
    int window_size;
} typedef sender_wnd;

struct rwnd
{
    int seq_num[256];
    int window_size;
} typedef reciever_wnd;

typedef struct
{
    bool isFree;
    pid_t pid;
    int udp_sock_fd;
    char *des_ip;
    int des_port;
    char *send_buffer[256];
    char *recv_buffer[256];
    sender_wnd swnd;
    reciever_wnd rwnd;
    pthread_mutex_t mutex;
} Shared_Mem;

int k_socket(int family, int type, int protocol);
int k_bind(int sockfd, const struct sockaddr *src_addr, socklen_t src_len, const struct sockaddr *dest_addr, socklen_t dest_len);

int k_sendto(int sock_fd, const void *message, size_t message_len, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);

int k_recvfrom(int sock_fd, void *buffer, size_t buffer_len, int flags, struct sockaddr *src_addr, socklen_t *src_len);
int k_close(int sock_fd);
int dropMessage(float p);