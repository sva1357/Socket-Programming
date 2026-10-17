#ifndef VIRTUAL_NODE_H
#define VIRTUAL_NODE_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern int udpfd;

struct neighbour{
    char name;
    unsigned char ip[4];
    int port;
    int cost;
};

struct lsp_packet{
    char sender_name;
    uint32_t seq_no;
    vector<struct neighbour> neighbours;
};

struct arguments{
    struct lsp_packet* packets;
    vector<struct neighbour>* neighbours; //if config file changes we need to update those changes accordingly and also saves memory 
    int packet_cnt;
};
#endif