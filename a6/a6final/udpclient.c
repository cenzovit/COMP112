/* simple tcp client based upon Stevens examples 
   Source: Stevens, "Unix Network Programming" */ 

#include <string.h>
#include <stdlib.h> 
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define MAX_MESG 80
#define MAX_ADDR 80
#define SERV_UDP_PORT 8000

int main(int argc, char **argv)
{
    int	sockfd;
    struct sockaddr_in	serv_addr;
    char *send_line = "hello there"; 
    struct hostent *host_ptr; 
    int serv_udp_port = SERV_UDP_PORT; 

    if (argc != 3) { 
	fprintf(stderr,"usage: udpclient <hostname> <port>\n");
        exit(1); 
    } 

    /* get the IP address of the host */
    if((host_ptr = gethostbyname(argv[1])) == NULL) {
        perror("gethostbyname error");
        exit(1);
    }
  
    if(host_ptr->h_addrtype !=  PF_INET) {
       fprintf(stderr,"unknown address type\n");
       exit(1);
    }
  
    /* get the port number */ 
    if (sscanf(argv[2],"%d",&serv_udp_port)==0) { 
       fprintf(stderr,"invalid port number\n");
       exit(1);
    } 
    /* construct an address for the socket 
       consisting of the address and port of the host */ 
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = PF_INET;
    /* address from gethostbyname */ 
    serv_addr.sin_addr.s_addr = 
       ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;
    /* port from command line or data */ 
    serv_addr.sin_port = htons(serv_udp_port);

    sockfd = socket(PF_INET, SOCK_DGRAM, 0);

    sendto(sockfd, send_line, strlen(send_line), 0, 
	(struct sockaddr *)&serv_addr, sizeof(serv_addr)); 

}
