#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

#define oracle_port 5000

void find_ip(struct sockaddr_in* my_addr){

}

int main(int argc, char* argv[]){
    if(argc!=3){
        cerr<<"Usage ./vn ORACLE_IP  OWN_UDP_PORT\n";
    }
    char* oracle_ipaddr=argv[1];
    int udp_port=atoi(argv[2]);
    int udpfd= socket(PF_INET,SOCK_DGRAM,0);
    if(udpfd==-1){
        cerr<<"Failed to create a udp socket\n";
    }
    struct sockaddr_in my_addr;
    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(udp_port);
    find_ip(&my_addr);
    memset(&(my_addr.sin_zero),'\0',8);
    if(bind(udpfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1){
        cerr<<"Failed to connect to udp port\n";
    }
    struct sockaddr_in oracle_addr;
    int tcpfd=socket(PF_INET,SOCK_STREAM,0);
    if(tcpfd==-1){
        cerr<<"Failed creating a tcp socket\n";
    }

    memset(&oracle_addr, 0, sizeof(oracle_addr));
    oracle_addr.sin_family = AF_INET;
    oracle_addr.sin_port = htons(oracle_port);
    oracle_addr.sin_addr.s_addr = inet_addr(oracle_ipaddr);

    if (connect(tcpfd, (struct sockaddr*)&oracle_addr, sizeof(struct sockaddr)) == -1) {
        perror("Failed to connect");
        close(tcpfd);
        close(udpfd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to Oracle....\n\n");



}