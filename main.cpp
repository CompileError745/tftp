#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUF_LEN 522

int main() {
    ssize_t n;
    char buffer[BUF_LEN];
    socklen_t sockaddr_len;
    int server_socket;
    struct sigaction act;
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    struct sockaddr_in sock_info;

    /* Set up interrupt handlers */
    act.sa_handler = sig_chld;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);

    act.sa_handler = sig_alarm;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGALRM, &act, NULL);

    sockaddr_len = sizeof(sock_info);

    /* Set up UDP socket */
    memset(&sock_info, 0, sockaddr_len);

    sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_info.sin_port = htons(69);
    sock_info.sin_family = PF_INET;

    if((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }

    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    /* Get port and print it */
    getsockname(server_socket, (struct sockaddr *)&sock_info, &sockaddr_len);

    printf("%d\n", ntohs(sock_info.sin_port));

    /* Receive the first packet and deal w/ it accordingly */
    while(1) {
        intr_recv:
        n = recvfrom(server_socket, buffer, BUF_LEN, 0,
                     (struct sockaddr *)&sock_info, &sockaddr_len);
        if(n < 0) {
            if(errno == EINTR) goto intr_recv;
            perror("recvfrom");
            exit(-1);
        }
        /* check the opcode */
        opcode_ptr = (unsigned short int *)buffer;
        opcode = ntohs(*opcode_ptr);
        if(opcode != RRQ && opcode != WRQ) {
            /* Illegal TFTP Operation */
            *opcode_ptr = htons(ERROR);
            *(opcode_ptr + 1) = htons(4);
            *(buffer + 4) = 0;
            intr_send:
            n = sendto(server_socket, buffer, 5, 0,
                       (struct sockaddr *)&sock_info, sockaddr_len);
            if(n < 0) {
                if(errno == EINTR) goto intr_send;
                perror("sendto");
                exit(-1);
            }
        }
        else {
            if(fork() == 0) {
                /* Child - handle the request */
                close(server_socket);
                break;
            }
            else {
                /* Parent - continue to wait */
            }
        }
    }

    if(opcode == RRQ) handle_read(&sock_info, buffer, BUF_LEN);
    if(opcode == WRQ) handle_write(&sock_info, buffer, BUF_LEN);

    return 0;
}