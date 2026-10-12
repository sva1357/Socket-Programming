#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEST_PORT 5000


int main(int argc, char *argv[])
{
    int udp_fd,tcp_fd;
    struct sockaddr_in my_addr,on_addr;

    udp_fd = socket(PF_INET,SOCK_DGRAM,0);

    int MYPORT = atoi(argv[2]);
    struct Neighbour {
    char node_id;          // A, B, C, D, E
    struct in_addr ip;     // Neighbour's IPv4 address
    unsigned short port;   // Neighbour's UDP port
    int cost;              // Link cost
    };
    struct Neighbour neighbours[26];

    memset(&my_addr,0,sizeof(my_addr));
    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(MYPORT);
    my_addr.sin_addr.s_addr=INADDR_ANY;

    bind(udp_fd, (struct sockaddr*)&my_addr,sizeof(struct sockaddr));
    tcp_fd=socket(PF_INET,SOCK_STREAM,0)
    memset(&on_addr, 0, sizeof(on_addr));
    on_addr.sin_family = PF_INET;
    on_addr.sin_port = htons(DEST_PORT);

    if (inet_pton(AF_INET, argv[1], &on_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid Oracle Node IP address\n");
        close(tcp_fd);
        close(udp_fd);
        return 1;
    }
    connect(tcp_fd, (struct sockaddr*)&on_addr,sizeof(struct sockaddr));

    int addrlen;
    char buf[100];

    addrlen = sizeof(struct sockaddr_in);

    int n = recvfrom(tcp_fd,buf,sizeof(buf)-1,0,(struct sockaddr*)&op_addr,&addrlen);

    printf("%s\n", buf);
    close(sockfd);

    return 0;
}