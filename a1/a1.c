/* simple tcp server based upon Stevens examples 
   Source: Stevens, "Unix Network Programming" */ 

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <stdlib.h> 
#include <signal.h> 

#define SERV_TCP_PORT 9000 /* server's port number */
#define MAX_MESG 1000	   /* longest message */ 
#define MAX_ADDR 80	   /* longest IP address */

struct index{
  char VN[MAX_MESG];
  char ipa[MAX_MESG];
  char usrAgent[MAX_MESG];
}visitorTable[10];

int trackVN = 0;


/* Write user agent after replacing unsafe characters */

/* From: The C Programming Language (K&R) */
/* strindex: return index of t in s, -1 if none */
int strindex(char s[], char t[])
{
  int i, j, k;
  for (i = 0; s[i] != '\0'; i++) {
    for (j=i, k=0; t[k]!='\0' && s[j]==t[k]; j++, k++)
      
      ;
    if (k > 0 && t[k] == '\0')
      return i;
  }
  return -1;
}

/* Store the IP and usr agent (safely) of the visitor */
void storeVisitor(char* ipa, char* message){
  /* locate beginning of User-Agent */
  int loc = strindex(message, "User-Agent: ") + 12;
  char usrAgentTemp[MAX_MESG];
  strncpy(usrAgentTemp, message+loc, strlen(message));
  
  /* locate end of the User-Agent */
  loc = strindex(usrAgentTemp, "\n");
  char usrAgent[MAX_MESG];
  strncpy(usrAgent, usrAgentTemp, loc);
  
  /* iterate through the unsafe User-Agent a character at a time, and create a new safe User-Agent */ 
  int i;
  char safeUsrAgent[MAX_MESG];
  strcpy(safeUsrAgent, "");
  for(i = 0; i < strlen(usrAgent); i++){
    if(usrAgent[i] == '>'){
      strcat(safeUsrAgent, "&gt;");
    }
    else if(usrAgent[i] == '<'){
      strcat(safeUsrAgent, "&lt;");
    }
    else if(usrAgent[i] == '\"'){
      strcat(safeUsrAgent, "&quot;");
    }
    else{
      char temp[2];
      temp[0] = usrAgent[i];
      temp[1] = '\0';
      strcat(safeUsrAgent, temp);
    }
  }

  /* if there have been more than 10 visitors, shift the list up and add to the end */
  if(trackVN > 9){
      int i;
      for(i = 0; i < 9; i++){
	visitorTable[i] = visitorTable[i+1];
      }
      strcpy(visitorTable[9].ipa, ipa);
      sprintf(visitorTable[9].VN, "%d", trackVN+1);
      strcpy(visitorTable[9].usrAgent, safeUsrAgent); 
  }
  else{
    strcpy(visitorTable[trackVN].ipa, ipa);
    sprintf(visitorTable[trackVN].VN, "%d", trackVN+1);
    strcpy(visitorTable[trackVN].usrAgent, safeUsrAgent); 
  }

  /* increment visitor number */
  trackVN++;
}

/* function to write the table of visitors in HTML */
void writeTable(int newsockfd){
  /* HTML tags in C */
  char tableTag[MAX_MESG];
  strcpy(tableTag, "<table border=\"1\">\n");
  char tableCloseTag[MAX_MESG];
  strcpy(tableCloseTag, "</table>\n");
  char tdTag[MAX_MESG];
  strcpy(tdTag, "<td>");
  char tdCloseTag[MAX_MESG];
  strcpy(tdCloseTag, "</td>\n");
  char tableRowTag[MAX_MESG];
  strcpy(tableRowTag, "<tr>\n");
  char tableRowCloseTag[MAX_MESG];
  strcpy(tableRowCloseTag, "</tr>\n");
  char header[MAX_MESG];
  strcpy(header, "<tr><th>Visitor Number</th><th>IP Address</th><th>User Agent</th></tr>\n");
  
  write(newsockfd, tableTag, strlen(tableTag));
  write(newsockfd, header, strlen(header));
  int i;
  for(i = 0; i < 10; i++){
    /* Creat new row */
    write(newsockfd, tableRowTag, strlen(tableRowTag));
    /* number */
    write(newsockfd, tdTag, strlen(tdTag));
    write(newsockfd, visitorTable[i].VN, strlen(visitorTable[i].VN));
    write(newsockfd, tdCloseTag, strlen(tdCloseTag));
    /* IP Address */
    write(newsockfd, tdTag, strlen(tdTag));
    write(newsockfd, visitorTable[i].ipa, strlen(visitorTable[i].ipa));
    write(newsockfd, tdCloseTag, strlen(tdCloseTag));
    /* User Agent */
    write(newsockfd, tdTag, strlen(tdTag));
    write(newsockfd, visitorTable[i].usrAgent, strlen(visitorTable[i].usrAgent));
    write(newsockfd, tdCloseTag, strlen(tdCloseTag));
    /* Close the row */
    write(newsockfd, tableRowCloseTag, strlen(tableRowCloseTag));
  }
  write(newsockfd, tableCloseTag, strlen(tableCloseTag));
}

/* create and write the response to the stream */
void cawr(int newsockfd){
  char genericResponse[MAX_MESG];
  strcpy(genericResponse, "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
  char htmlTag[MAX_MESG];
  strcpy(htmlTag, "<html>\n");
  char htmlCloseTag[MAX_MESG];
  strcpy(htmlCloseTag, "</html>\0");
    
  write(newsockfd,genericResponse,strlen(genericResponse)); 
  write(newsockfd, htmlTag, strlen(htmlTag));
  writeTable(newsockfd);
  write(newsockfd, htmlCloseTag, strlen(htmlCloseTag));
}

int main(int argc, char *argv[])
{
  
  /* server */ 
  int sockfd, newsockfd; 
  struct sockaddr_in serv_addr;
  int serv_port = SERV_TCP_PORT;
  
  /* message */ 
  char message[MAX_MESG]; 
  int mesglen; int i; 
  
  /* client */
  struct sockaddr_in cli_addr;        /* raw client address */
  char cli_dotted[MAX_ADDR];          /* message ip address */
  struct hostent *cli_hostent;        /* host entry */
  
  /* Parse Command Line Arguments */ 
  if(argc != 2) { 
    fprintf(stderr, "%s: usage: %s port\n",argv[0], argv[0]); 
    exit(1); 
  } 
  
  sscanf(argv[1], "%d", &serv_port); /* read the port number if provided */
  
  /* open a TCP socket (an Internet stream socket) */
  if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("can't open stream socket");
    exit(1);
  }
  
  /* bind the local address, so that the client can send to server */
  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  /* initialize to zero */ 
  serv_addr.sin_family = PF_INET;	/* internet domain addressing */ 
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  /* source address is local */ 
  serv_addr.sin_port = htons(serv_port); 
  /* port is given one */ 
  
  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("can't bind local address");
    exit(1);
  }
  
  /* listen to the socket */
  listen(sockfd, 5);
  fprintf(stderr, "%s: server listening on port %d\n", argv[0], serv_port); 
  
  for(;;) {
    
    /* wait for a connection from a client; this is an iterative server */
    mesglen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &mesglen);
    
    if(newsockfd < 0) {
      perror("can't bind local address");
    }
    
    /* get numeric internet address */ 
    inet_ntop(PF_INET, (void *)&(cli_addr.sin_addr), cli_dotted, MAX_MESG); 
    fprintf(stderr, "%s: server connection from %s\n", argv[0], cli_dotted); 

    /* read a message from the client */
    mesglen = read(newsockfd, message, MAX_MESG); 
    /* make sure it's a proper message */
    message[mesglen] = '\0';
    fprintf(stderr, "%s: server receives: %s\n", argv[0], message);

    /* store visitor */
    storeVisitor(cli_dotted, message);

    /* create and write the proper response */
    cawr(newsockfd);
    
    /* push stream to the client */
    close(newsockfd);
  }  
}
