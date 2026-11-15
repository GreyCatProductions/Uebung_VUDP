#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "VUDP.h"

int make_socket(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(VUDP_PORT);               //Ziel-Port
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  //Ziel-IPv4

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void send_ack(int sockfd, uint32_t seq,
              const struct sockaddr_in *client, socklen_t sock_len, int loss)
{
    if (rand() % 100 < loss){
        printf("[SERVER] ACK seq=%u DROPPED\n", seq);
        return;
    }

    struct vudp_pck ack;
    ack.flags = VUDP_FLAG_ACK;
    ack.seq = htonl(seq);

    sendto(sockfd, &ack, VUDP_HDR_LEN, 0,
           (const struct sockaddr *)client, sock_len);
}

void process_package(int sockfd, const struct vudp_pck *pck, size_t pyl_len,
                     const struct sockaddr_in *client, socklen_t sock_len,
                     uint32_t *seq_exp, int loss)
{
    uint32_t seq = ntohl(pck->seq);

    if (seq == *seq_exp){ //New package
        printf("[SERVER] seq=%u: %.*s\n", seq, (int)pyl_len, pck->payload);
        (*seq_exp)++;
    }
    else if (seq < *seq_exp){ //Old package -> ACK not arrived
        printf("[SERVER] duplicate seq=%u -> resend ACK\n", seq);
    }
    else
    { //Newer than expected
        printf("[SERVER] seq=%u but expected %u. Ignroed", seq, *seq_exp);
        return;
    }

    send_ack(sockfd, seq, client, sock_len, loss);
}

//Takes number in % as input for simulated package loss
int main(int argc, char *argv[])
{
    int loss = (argc > 1) ? atoi(argv[1]) : 0;

    int sockfd = make_socket();
    if (sockfd < 0)
        return EXIT_FAILURE;

    printf("[SERVER] listens for UDP on %b:%d\n", INADDR_LOOPBACK, VUDP_PORT);

    uint32_t seq_exp = 0;
    while (1)
    {
        struct vudp_pck pck;
        struct sockaddr_in client;
        socklen_t sock_len = sizeof(client);

        // blocking
        ssize_t size = recvfrom(sockfd, &pck, sizeof(pck), 0, (struct sockaddr *)&client, &sock_len);

        if(size == -1){
            perror("recvfrom");
            return EXIT_FAILURE;
        }

        if (size < (ssize_t)VUDP_HDR_LEN){ // header kaputt
            printf("[SERVER] malformed packet (%zd bytes) -> ignored\n", size);
            continue;
        }

        if (rand() % 100 < loss){
            printf("[Server] Paket VERWORFEN\n");
            continue;
        }

        if (pck.flags == VUDP_FLAG_FIN){ // client is done
            printf("[SERVER] FIN received -> ready for next client\n");
            seq_exp = 0;
            send_ack(sockfd, ntohl(pck.seq), &client, sock_len, loss);
            continue;
        }

        if (pck.flags != VUDP_FLAG_DATA){ // unknown packet type
            continue;
        }

        size_t pyl_len = size - VUDP_HDR_LEN;
        process_package(sockfd, &pck, pyl_len, &client, sock_len, &seq_exp, loss);
    }
}