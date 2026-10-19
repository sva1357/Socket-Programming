#include <pthread.h>
#include<vector>
#include <netdb.h>      // gethostbyname(), struct hostent
#include <unistd.h>   
#include <sys/time.h> 
#include <sys/select.h>
#include <bits/stdc++.h>
#include "virtual_node.h"

using namespace std;
int udpfd;
int tcpfd;
#define SEND_INTERVAL 10
void find_myip(struct sockaddr_in* my_addr){
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct hostent* host = gethostbyname(hostname);

    if (host == nullptr) {
        perror("gethostbyname failed");
        exit(1);
    }
    memset(my_addr, 0, sizeof(*my_addr));
    my_addr->sin_family = AF_INET;
    memcpy(&my_addr->sin_addr, host->h_addr_list[0],
           sizeof(my_addr->sin_addr));

}

void format(in_addr ip, int port, char* info){
    memcpy(info, &ip,4);
    uint16_t p=htons((uint16_t)port);
    memcpy(info+4,&p,2);
    return;
}


void serialize_lsp(struct lsp_packet *pkt, int *len, char **msg) {

    if (pkt == nullptr || len == nullptr || msg == nullptr) {
        return;
    }
    int neighbour_count = pkt->neighbours.size();
    *len = 1 + 4 + 4 + neighbour_count * 11;
    *msg = (char *)malloc(*len);
    if (*msg == nullptr) {
        *len = 0;
        return;
    }
    char *ptr = *msg;
    memcpy(ptr, &pkt->sender_name, 1);
    ptr = ptr + 1;
    uint32_t sequence_number = htonl(pkt->seq_no);
    memcpy(ptr, &sequence_number, 4);
    ptr = ptr + 4;
    uint32_t count = htonl(neighbour_count);
    memcpy(ptr, &count, 4);
    ptr = ptr + 4;

    for (int i = 0; i < neighbour_count; i++) {
        struct neighbour n = pkt->neighbours[i];
        memcpy(ptr, &n.name, 1);
        ptr = ptr + 1;
        memcpy(ptr, n.ip, 4);
        ptr = ptr + 4;
        uint16_t port = htons(n.port);
        memcpy(ptr, &port, 2);
        ptr = ptr + 2;
        uint32_t cost = htonl(n.cost);
        memcpy(ptr, &cost, 4);
        ptr = ptr + 4;
    }
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
            for(size_t j=0; j< args->neighbours->size(); j++){
                if((*(args->neighbours))[j].name==pkt->sender_name) continue;
                struct neighbour *n=&((*(args->neighbours))[j]);
                struct sockaddr_in dest_addr={};
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

bool deserialize_lsp(const char* msg, int len, lsp_packet* pkt) {
    if (msg == nullptr || pkt == nullptr || len < 9) {
        return false;
    }

    const char* ptr = msg;
    const char* end = msg + len;

    pkt->sender_name = *ptr++;

    uint32_t seq;
    memcpy(&seq, ptr, 4);
    pkt->seq_no = ntohl(seq);
    ptr += 4;

    uint32_t count;
    memcpy(&count, ptr, 4);
    count = ntohl(count);
    ptr += 4;

    if (count > static_cast<uint32_t>((end - ptr) / 11)) {
        return false;
    }

    pkt->neighbours.clear();

    for (uint32_t i = 0; i < count; i++) {
        neighbour n{};

        n.name = *ptr++;

        memcpy(n.ip, ptr, 4);
        ptr += 4;

        uint16_t port;
        memcpy(&port, ptr, 2);
        n.port = ntohs(port);
        ptr += 2;

        uint32_t cost;
        memcpy(&cost, ptr, 4);
        n.cost = static_cast<int>(ntohl(cost));
        ptr += 4;

        pkt->neighbours.push_back(n);
    }

    // Reject packets containing unexpected trailing bytes.
    return ptr == end;
}

void* receiver_thread(void* arg){
    struct arguments* args = (struct arguments*)arg;

    char buffer[2048];

    while (1) {
        struct sockaddr_in src_addr = {};
        socklen_t addr_len = sizeof(src_addr);
        int len = recvfrom(
            udpfd,
            buffer,
            sizeof(buffer),
            0,
            (struct sockaddr*)&src_addr,
            &addr_len
        );

        if (len < 0) {
            cerr << "recvfrom failed\n";
            continue;
        }
        struct lsp_packet pkt;

        if (!deserialize_lsp(buffer, len, &pkt)) {
            cerr << "Invalid LSP packet received\n";
            continue;
        }
        int slot = -1;

        for (int i = 0; i < args->packet_cnt; i++) {
            if (args->packets[i].sender_name == pkt.sender_name) {
                slot = i;
                break;
            }

            if (slot == -1 &&
                args->packets[i].sender_name == 0) {
                slot = i;
            }
        }

        if (slot == -1) {
            cerr << "No available packet slot\n";
            continue;
        }

        if (args->packets[slot].sender_name == pkt.sender_name &&
            args->packets[slot].seq_no >= pkt.seq_no) {
            continue;
        }

        args->packets[slot] = pkt;
    }

    return NULL;

}
int receive(int sock_fd, char* neigh_info, int expected_bytes){
    int received=0;

    while(received<expected_bytes){
        int r=recv(sock_fd, neigh_info+received,expected_bytes-received,0);
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
    
    if (packet->sender_name == 0)
        return;
    int len;
    char* msg;
    serialize_lsp(packet, &len, &msg);
    for (size_t i = 0; i < neighbours.size(); i++) {

        struct neighbour* n = &neighbours[i];
        if (n->name == packet->sender_name)
            continue;

        struct sockaddr_in dest_addr = {};

        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(n->port);

        if (inet_pton(AF_INET, (const char*)n->ip,
                      &dest_addr.sin_addr) != 1) {
            cerr << "Invalid neighbor IP address\n";
            continue;
        }

        if (sendto(udpfd,msg,len,0,(struct sockaddr*)&dest_addr,sizeof(dest_addr)) < 0) {
            perror("sendto failed");
        }
    }

    free(msg);
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
    char mynode_name = 'A';
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
    for(size_t i=0; i<neighbours.size(); i++){
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
        if(i==packet.sender_name- 'A') continue;
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
 if (total_packets[i].sender_name != 0) {
        cout << "Node: "
             << total_packets[i].sender_name << '\n';

        cout << "Sequence number: "
             << total_packets[i].seq_no << '\n';

        cout << "Neighbors:\n";

        for (const auto& n : total_packets[i].neighbours) {
            char ip_str[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, n.ip, ip_str, sizeof(ip_str));

            cout << "  Node: " << n.name
                 << ", IP: " << ip_str
                 << ", Port: " << n.port
                 << ", Cost: " << n.cost << '\n';
        }
    }
    close(tcpfd);
    close(udpfd);



}
}
