#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "VUDP.h"

#define TIMEOUT_MS 500
#define MAX_RETRIES 5

int send_reliable(int sockfd, const struct sockaddr_in *server,
                  uint8_t flags, uint32_t seq, const char *data, size_t len)
{
    struct vudp_pck pck;
    pck.flags = flags;
    pck.seq = htonl(seq);
    memcpy(pck.payload, data, len);

    for (int try = 1; try <= MAX_RETRIES; try++)
    {
        sendto(sockfd, &pck, VUDP_HDR_LEN + len, 0,
               (const struct sockaddr *)server, sizeof(*server));

        struct vudp_pck ack;
        ssize_t size = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

        if (size < 0)
        {
            printf("[CLIENT] timeout seq=%u -> resend\n", seq);
            continue;
        }

        if (size == (ssize_t)VUDP_HDR_LEN && ack.flags == VUDP_FLAG_ACK && ntohl(ack.seq) == seq)
        {
            printf("[CLIENT] ACK seq=%u received\n", seq);
            return 1;
        }
    }

    printf("[CLIENT] no ACK for seq=%u after %d tries -> giving up\n", seq, MAX_RETRIES);
    return 0;
}

void send_malformed(int sockfd, const struct sockaddr_in *server)
{
    char bad[2] = {VUDP_FLAG_DATA, 0x42};
    sendto(sockfd, bad, sizeof(bad), 0,
           (const struct sockaddr *)server, sizeof(*server));
    printf("[CLIENT] sent malformed packet (%zu bytes)\n", sizeof(bad));
}

int main(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        perror("socket");
        return EXIT_FAILURE;
    }

    // Timeout on socket
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT_MS * 1000;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
        perror("setsocketopt");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(VUDP_PORT);
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    printf("[CLIENT] type a line and press Enter (Ctrl+D to quit)\n");
    printf("[CLIENT] commands: /seq N  -> next packet uses seq N\n");
    printf("[CLIENT]           /bad    -> send a malformed packet\n");

    uint32_t seq = 0;
    char send_buffer[VUDP_MAX_PYL];
    int server_alive = 1;

    // fgets returned bei ctrl d NULL
    while (fgets(send_buffer, sizeof(send_buffer), stdin) != NULL) //BLOCKING
    {
        size_t len = strcspn(send_buffer, "\n");
        send_buffer[len] = '\0';

        if (strncmp(send_buffer, "/seq", 4) == 0)
        {
            seq = (uint32_t)atoi(send_buffer + 4);
            printf("[CLIENT] next seq is now %u\n", seq);
        }
        else if (strcmp(send_buffer, "/bad") == 0)
        {
            send_malformed(sockfd, &server);
        }
        else if (send_reliable(sockfd, &server, VUDP_FLAG_DATA, seq, send_buffer, len))
        {
            seq++;
        }
        else
        {
            printf("[CLIENT] server not responding.\n");
            server_alive = 0;
            break;
        }
    }

    if (server_alive)
        send_reliable(sockfd, &server, VUDP_FLAG_FIN, seq, "", 0);

    close(sockfd);
    return 0;
}
