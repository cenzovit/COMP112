/* simple tcp client based upon Stevens examples 
   Source: Stevens, "Unix Network Programming" */ 

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "shared.h"

int main(int argc, char **argv)
{
    int	sockfd;
    struct sockaddr_in	recv_addr;
    char *send_line = NULL; 

    if (argc != 3) { 
	fprintf(stderr,"%s usage: %s port message\n", argv[0], argv[0]);
        exit(1); 
    } 
    /* set target address */ 
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = PF_INET;
    recv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    // inet_pton(PF_INET, "130.64.23.255", &recv_addr.sin_addr);
    recv_addr.sin_port = htons(atoi(argv[1]));
    send_line = argv[2]; 

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);

/* allow broadcasts */ 
    const int broadcast=1;
    if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,
		&broadcast,sizeof broadcast)))
    {
	perror("setsockopt - SO_SOCKET ");
	exit(1); 
    } 

    if (sendto(sockfd, send_line, strlen(send_line), 0, 
	(struct sockaddr *)&recv_addr, sizeof(recv_addr))==(-1)) 
	perror("sendto"); 

}
