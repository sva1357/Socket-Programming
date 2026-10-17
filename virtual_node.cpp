#include <pthread.h>
#include <sys/time.h> 
#include <sys/select.h>
#include <bits/stdc++.h>
#include "virtual_node.h"

using namespace std;
int udpfd;
int tcpfd;
#define SEND_INTERVAL 10
void find_myip(struct sockaddr_in* my_addr){

}

void format(in_addr ip, int port, char* info){
    memcpy(info, &ip,4);
    uint16_t p=htons((uint16_t)port);
    memcpy(info+4,&p,2);
    return;
}


void serialize_lsp(struct lsp_packet *pkt,int* len, char** msg){

}
void* sender_thread(void* arg){
    struct arguments* args=(struct arguments*)arg;
    while(1){
        for(int i=0; i<args->packet_cnt; i++){
            struct lsp_packet *pkt=&args->packets[i];
            if(pkt->sender_name==0) continue;
            int len;
            char* msg;
            serialize_lsp(pkt, &len,&msg);
            for(int j=0; j< args->neighbours->size(); j++){
                if((*(args->neighbours))[j].name==pkt->sender_name) continue;
                struct neighbour *n=&((*(args->neighbours))[j]);
                struct sockaddr_in dest_addr={0};
                dest_addr.sin_family=AF_INET;
                dest_addr.sin_port=htons(n->port);
                inet_pton(AF_INET,(const char*)n->ip,&dest_addr.sin_addr);
                if(sendto(udpfd,msg,len,0, (struct sockaddr*)&dest_addr,sizeof(dest_addr))<0){
                    cerr<<"sento failed\n";
                }

            }
            free(msg);
        }
        sleep(SEND_INTERVAL);
    }
    return NULL;
}
void* receiver_thread(void* arg){

}
int receive(int sock_fd, char* neigh_info, int expected_bytes){
    int received=0;
    int r;
    while(r<expected_bytes){
        r=recv(sock_fd, neigh_info+received,expected_bytes-received,0);
        if(r<=0){
            cerr<<"recv failed\n";
            close(tcpfd);
            close(udpfd);
            return 0;
        }
        received+=r;
    }
    return received;
}
void send_lsp_packet(struct lsp_packet* packet, int udpfd, vector<neighbour>& neighbours){

}
void recover_neigh(char* neigh_info, vector<neighbour>& neighbours){
    uint32_t sz;
    memcpy(&sz,neigh_info,4);
    uint32_t msg_size=ntohl(sz);
    char* ptr=neigh_info+4;
    char* end=neigh_info+4+msg_size;
    while(ptr+11<=end){
        struct neighbour n;
        n.name=*ptr;
        ptr+=1;
        memcpy(n.ip,ptr,4);
        ptr+=4;
        uint16_t port;
        memcpy(&port,ptr,2);
        n.port=ntohs(port);
        ptr+=2;
        uint32_t cost;
        memcpy(&cost,ptr,4);
        n.cost=ntohl(cost);
        ptr+=4;
        neighbours.push_back(n);
    }
}

int main(int argc, char* argv[]){
    if(argc!=4){
        cerr<<"wrong usage\n";
        return 0;
    }
    char mynode_name;
    char* oracle_ipaddr=argv[1];
    int oracle_port=atoi(argv[2]);
    int udp_port=atoi(argv[3]);
    udpfd=socket(PF_INET,SOCK_DGRAM,0);
    if(udpfd==-1){
        cerr<<"Failed creating udp socket\n";
        return 0;
    }
    struct sockaddr_in my_addr;
    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(udp_port);
    find_myip(&my_addr);
    memset(&(my_addr.sin_zero),'\0',8);
    if(bind(udpfd,(struct sockaddr*) &my_addr, sizeof(struct sockaddr))==-1){
        cerr<<"Failed to connect to udp port\n";
        close(udpfd);
        return 0;
    }
    struct sockaddr_in oracle_addr;
    tcpfd=socket(PF_INET,SOCK_STREAM,0);
    if(tcpfd==-1){
        cerr<<"Failed creating a tcp socket\n";
        return 0;
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
    cout<<"Connecting to Oracle...\n";
    vector<neighbour> neighbours;
    char info[50];
    format(my_addr.sin_addr,udp_port,info);
    while(send(tcpfd,info,6,0)==-1){
        if(errno==EINTR) continue;
        cerr<<"Failed to send node info over TCP\n";
        break;
    }
    char neighbour_info[280];
    receive(tcpfd,neighbour_info,280);
    sleep(1);
    recover_neigh(neighbour_info, neighbours);
    for(int i=0; i<neighbours.size(); i++){
        cout<<"Neighbour "<<i+1<<"\n";
        cout<<"Node "<<neighbours[i].name<<": "<<neighbours[i].ip<<" Port= "<<neighbours[i].port<<" cost= "<<neighbours[i].cost<<endl;
    }
    cout<<endl;
    struct lsp_packet total_packets[26];
    struct lsp_packet packet;
    packet.sender_name=mynode_name;
    packet.seq_no=0;
    packet.neighbours=neighbours;
    total_packets[packet.sender_name-'A']=packet;
    for(int i=0; i<26; i++){
        if(i==packet.sender_name) continue;
        total_packets[i].sender_name=0;
        total_packets[i].seq_no=0;
    }
    send_lsp_packet(&packet,udpfd,neighbours);
    struct arguments args;
    args.packets = total_packets;
    args.packet_cnt = 26;  
    args.neighbours = &neighbours;

    pthread_t sender_tid,receiver_tid;

    pthread_create(&sender_tid, NULL, sender_thread, &args);
    pthread_create(&receiver_tid, NULL, receiver_thread, &args);

    pthread_join(sender_tid, NULL);
    pthread_join(receiver_tid, NULL);
    for(int i=0;i<26;i++){
        if(total_packets[i].sender_name){
        }
    }
    close(tcpfd);
    close(udpfd);




}