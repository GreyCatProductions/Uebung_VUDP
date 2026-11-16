#pragma once 

#include <stdint.h>
#include <arpa/inet.h>

#define VUDP_PORT       1337
#define VUDP_MAX_PYL    1400
#define VUDP_FLAG_DATA  0x01   
#define VUDP_FLAG_ACK   0x02  
#define VUDP_FLAG_FIN   0x04   

#define VUDP_HDR_LEN (sizeof(struct vudp_pck) - VUDP_MAX_PYL)

struct __attribute__((packed)) vudp_pck
{
    uint8_t     flags;
    uint32_t    seq;
    char        payload[VUDP_MAX_PYL];
};

//ACK package must not contain any data
//Every package must be answered with an ACK before a new is sent
//Sender shall resend package if no ACK is received in a certain time
//Sender may stop sending if no ACK come back

