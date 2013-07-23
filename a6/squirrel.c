/* simple tcp server based upon Stevens examples 
   Source: Stevens, "Unix Network Programming" */ 

#include <stdio.h>
#include <unistd.h> 
#include <stdlib.h> 
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <time.h>
#include "operations.h"
#include "storage.h" 


/* logging of server actions */ 
#define MAXOUT 256 		/* maximum number of output chars for flog */ 

static void flog(const char *fmt, ...) {
    va_list ap;
    char *p; 
    char buf[MAXOUT]; 
    va_start(ap, fmt);
    fprintf(stderr,"[squirrel: "); 
    vsnprintf(buf,MAXOUT,fmt,ap); 
    for (p=buf; *p && p-buf<MAXOUT; p++) 
	if ((*p)>=' ') 
	    putc(*p,stderr); 
    fprintf(stderr,"]\n"); 
    va_end(ap);
} 

//function which prints the IP list to stderr
void printList(struct index *index){
  if(index->next != NULL){
    printList(index->next);
  }
  fprintf(stderr, "IP: %s\n", index->ipa);
}

//function which frees the IP list
void freeList(struct index *index){
  if(index->next != NULL){
    freeList(index->next);
  }
  free(index);
}

//Fuction to "start" a timer
void startTimer(time_t *timer){
  *timer = time(NULL);
}

//Function to check if a given amount of time has passed since
//  the timer has started.
int checkTimer(time_t *timer, int check){
  if((difftime(time(NULL), *timer) > check)){
    *timer = time(NULL);
    return 1;   
  }
  else{
    return 0;
  }
}

//Function which returns the number of active servers (by interating
//  through the IP linked list)
int numServers(struct index* root){
  int servers = 0;
  while(root->next != NULL){
    root = root->next;
    servers = servers + 1;
  }
  return servers;
}

//Taken from send-broadcast.c - rewritten as a function
void sendBroadcast(int port){
  int sockfd;
  struct sockaddr_in recv_addr;
  char *send_line = NULL; 
  
  /* set target address */ 
  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family = PF_INET;
  recv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  // inet_pton(PF_INET, "130.64.23.255", &recv_addr.sin_addr);
  recv_addr.sin_port = htons(port);
  send_line = "ping"; 
  
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  
  /* allow broadcasts */ 
  const int broadcast=1;
  if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,
		 &broadcast,sizeof broadcast))){
    perror("setsockopt - SO_SOCKET ");
    exit(1); 
  } 
  
  if (sendto(sockfd, send_line, strlen(send_line), 0, 
	     (struct sockaddr *)&recv_addr, sizeof(recv_addr))==(-1)){
    perror("sendto"); 
  }
}

/*****************************************************
  interruptable reads handle intervening udp requests 
 *****************************************************/

/* create a select call to fire udp or accept, as relevant */ 
int accept_or_udp(int tcpsock, struct sockaddr *addr, 
		  socklen_t *addrlen, int udpsock, int serv_port, struct index* root, struct fileList* listRoot) { 
  time_t timerOne;
  time_t timerTwo;
  fd_set rfds;
  fd_set wfds; 
  fd_set efds; 
  // return accept(tcpsock, addr, addrlen); 
  startTimer(&timerOne);
  startTimer(&timerTwo);
  while (TRUE) { 
    // Set up to use select() as a timeout for listening
    struct timeval stTimeOut;
    
    // Timeout of one second
    stTimeOut.tv_sec = 1;
    stTimeOut.tv_usec = 0;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds); 
    FD_SET(tcpsock, &rfds); 
    FD_SET(udpsock, &rfds); 
    FD_ZERO(&wfds); 
    FD_ZERO(&efds);

    int s = select((tcpsock>udpsock?tcpsock:udpsock)+1, &rfds, &wfds, &efds, &stTimeOut);
    
    // If select = -1, there was an error
    if(s == -1){
      fprintf(stderr, "Call to select() failed!");
    }
    // If select = 1, there was something to be read
    else if (s == 1)  {
      if (FD_ISSET(udpsock,&rfds)){
	udp(udpsock, root);
      }
      if (FD_ISSET(tcpsock,&rfds)){
	//if IP list is empty, it was just freed
	//  give the list a moment to repopulate
	if(root->next == NULL){
	  sendBroadcast(serv_port);
	  waitForPings(udpsock, root);
	  fprintf(stderr, "Current Active Servers: \n");
	  printList(root->next);
	}
	return accept(tcpsock,addr,addrlen); 
      }    
    } 
    // Select = 0, timeout reached
    else{
    }
        
    if(checkTimer(&timerOne, 1)){
      sendBroadcast(serv_port);
    }

    //reset list
    if(checkTimer(&timerTwo, 2)){
      if(root->next != NULL){
	freeList(root->next);
      }
      root->next = NULL;
    }
  } 
} 

/* create a select call to fire udp or read, as relevant */ 
ssize_t read_or_udp(int tcpsock, void *buf, size_t count, int udpsock, struct index* root) { 

    fd_set rfds;
    fd_set wfds; 
    fd_set efds; 

    while (TRUE) { 
	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds); FD_SET(tcpsock, &rfds); FD_SET(udpsock, &rfds); 
	FD_ZERO(&wfds); FD_ZERO(&efds); 
	if (select((tcpsock>udpsock?tcpsock:udpsock)+1, 
	    &rfds, &wfds, &efds, NULL))  {
	  if (FD_ISSET(udpsock,&rfds)) udp(udpsock, root);  
	    if (FD_ISSET(tcpsock,&rfds)) return read(tcpsock,buf,count); 
	} 
    } 
} 

/* read up to \n from a socket */ 
int read_line_or_udp(int tcpsock, char *stuff, int len, int udpsock, struct index* root) { 
    int size; 
    int count = 0; 
    while (count<len && (size=read_or_udp(tcpsock,stuff,1,udpsock, root))>0 
	   && *stuff != '\n') {
	stuff++; count++; 
    } 
    if (count<len && size>0) { stuff++; count++; } /* skip \n */ 
    if (count<len) { *stuff='\0'; } 
    return count; 
} 

/* read a fixed-size block from a socket, or up to eof */ 
int read_block_or_udp(int tcpsock, char *stuff, int size, int udpsock, struct index* root) { 
    int c; 
    int count=0; 
    do { 
      c=read_or_udp(tcpsock,stuff,size,udpsock, root); 
	if (c>0) { size-=c; stuff+=c; count+=c; } 
	else return count; 
    } while (size>0); 
    return count; 
} 

/*****************************************************
  parser for tcp commands from clients
 *****************************************************/

/* returns 1 if query is a get, 0 otherwise */ 
int req_is_get(char *request, char *filename) { 
    if (strncmp(request,"get ",4)==0) { 
	flog("request is 'get'"); 
	sscanf(request+4,"%s",filename); 
	flog("filename is '%s'\n",filename); 
	return 1; 
    } else { 
	return 0; 
    } 
} 

/* returns 1 if query is a get, 0 otherwise */ 
int req_is_put(char *request, char *filename, int *filesize) { 
    if (strncmp(request,"put ",4)==0) { 
	if (sscanf(request+4,"%s%d",filename,filesize)==2) { 
	    flog("request is 'put'"); 
	    flog("put: filename is '%s'",filename); 
	    flog("put: filesize is %d",*filesize); 
	    return 1; 
	} else { 
	    flog("put: garbled command"); 
	} 
    } 
    return 0; 
} 

/* if request is del, parses request, returns filename */ 
int req_is_del(char *request, char *filename) { 
    if (strncmp(request,"del ",4)==0) { 
	flog("request is 'del'"); 
	if (sscanf(request+4,"%s",filename)==1) { 
	    flog("del: filename is '%s'",filename); 
	    return 1; 
	} else { 
	    flog("del: garbled command"); 
	} 
    } else { 
	return 0; 
    } 
    return 0; 
} 

/*****************************************************
  acknowledgement messages for tcp commands 
 *****************************************************/

/* write out an acknowledge message */ 
void ok(int fd) { 
    char message[MAXMESG]; 
    strcpy(message,"100 ok\n");
    flog("sending '%s'", message);
    write(fd,message,strlen(message)); 
} 

/* write out an ok and a file */ 
void ok_get(int fd, const char *contents, int size) { 
    char message[MAXMESG]; 
    sprintf(message,"100 ok %d\n",size);
    flog("sending '%s'", message);
    write(fd,message,strlen(message)); 
    flog("sending file content (%d bytes)", size);
    write(fd,contents, size);
} 

/* write out a failure response */ 
void failed(int fd) { 
    char message[MAXMESG]; 
    strcpy(message,"200 failed\n");
    flog("sending '%s'", message);
    write(fd,message,strlen(message)); 
} 

/*****************************************************
  main program handles tcp requests and farms out udp 
 *****************************************************/

/* usage message */
void usage() { 
  fprintf(stderr, "squirrel usage: squirrel <port> <blocks>\n"); 
} 

int main(int argc, char *argv[])
{
  
  int errors=0; 
  
  /* generic data */ 
  serv_port = SERV_PORT;
  int storage_blocks=SERV_BLOCKS;
  
  /* tcp server */ 
  int tcp_sock, new_tcp_sock;
  struct sockaddr_in tcp_addr;
  
  /* udp server */ 
  int udp_sock;
  struct sockaddr_in udp_addr; 	/* server address */ 
  
  /* message */ 
  char request[MAXMESG]; 
  int reqlen; 
  
  /* file storage */ 
  char filename[MAXNAME]; 
  int filesize; 
  char *contents; 
  
  /* command line: server port_number max_blocks */
  
  if(argc != 3)  {
    flog("need exactly 2 arguments (got %d)",argc-1);
    usage(); 
    exit(1); 
  } 
  sscanf(argv[1], "%d", &serv_port);   /* read the port number */ 
  if (serv_port<8000 || serv_port>65000) { 
    flog("invalid port %d",serv_port); 
    errors++; 
  } 
  sscanf(argv[2], "%d", &storage_blocks); /* number of blocks to buffer */ 
  if (storage_blocks<=0) { 
    flog("invalid number of blocks %d",storage_blocks); 
    errors++; 
  } 
  flog("tcp server starting on port %d with %d blocks",serv_port,storage_blocks); 
  storage_begin(storage_blocks); 
  
  
  /* make a udp socket */ 
  udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
  
  bzero(&udp_addr, sizeof(udp_addr));
  udp_addr.sin_family      = PF_INET;
  udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  udp_addr.sin_port        = htons(serv_port);
  
  /* bind it to an address and port */ 
  if (bind(udp_sock, (struct sockaddr *) &udp_addr, sizeof(udp_addr))<0) 
    perror("can't bind local address"); 
  
  /* open a TCP socket (an Internet stream socket) */
  if((tcp_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("can't open stream socket");
    exit(1);
  }
  
  /* bind the local address, so that the client can send to server */
  bzero((char *) &tcp_addr, sizeof(tcp_addr));
  /* initialize to zero */ 
  tcp_addr.sin_family = PF_INET;	/* internet domain addressing */ 
  tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  /* source address is local */ 
  tcp_addr.sin_port = htons(serv_port); /* port is given one */ 
  
  if(bind(tcp_sock, (struct sockaddr *) &tcp_addr, sizeof(tcp_addr)) < 0) {
    perror("can't bind local address");
    exit(1);
  }
  
  /* listen to the socket */
  listen(tcp_sock, 5);
  
  //init linked list by creating a root handle
  struct index *root = malloc(sizeof(struct index));
  strcpy(root->ipa, ""); 
  root->next = NULL; 
  
  //init stored file list
  struct fileList *listRoot = malloc(sizeof(struct fileList));
  listRoot->next = NULL;
  
  sendBroadcast(serv_port);
  for(;;) {
    
    /* client */
    struct sockaddr_in cli_addr;        /* raw client address */
    char cli_dotted[MAXADDR];          /* message ip address */
    unsigned long cli_ulong; 		/* address in binary */ 
    struct hostent *cli_hostent;        /* host entry */
    
    /* wait for a connection from a client; this is an iterative server */
    reqlen = sizeof(cli_addr);
    new_tcp_sock = accept_or_udp(tcp_sock, (struct sockaddr *) &cli_addr, &reqlen,udp_sock, serv_port, root, listRoot);
    
    if(new_tcp_sock < 0) {
      perror("can't bind local address");
    }
    
    /* get numeric internet address */ 
    inet_ntop(PF_INET, (void *)&(cli_addr.sin_addr),  cli_dotted, MAXMESG); 
    flog("connection from %s",cli_dotted); 
    
    /* convert numeric internet address to name */ 
    cli_ulong = cli_addr.sin_addr.s_addr; 
    cli_hostent = gethostbyaddr((char *)&cli_ulong, sizeof(cli_ulong),PF_INET);
    if (cli_hostent) { 
      flog("host name is '%s'", cli_hostent->h_name); 
    } else { 
      flog("no name for host"); 
    } 	
    
    /* read a message from the client */
    reqlen = read_line_or_udp(new_tcp_sock, request, MAXMESG, udp_sock, root); 
    flog("received '%s'", request);
    if (req_is_get(request,filename)) { 
      if (getFromServers(filename, &contents, &filesize, root, listRoot, udp_sock, serv_port)) { 
	ok_get(new_tcp_sock,contents,filesize);
	free(contents);
      } else { 
	failed(new_tcp_sock); 
      } 
    } else if (req_is_put(request,filename,&filesize)) { 
      flog("put: reading file contents (%d bytes)",filesize); 
      contents = (char *)malloc(filesize); 	
      int bytesread = 0; 
      if ((bytesread=read_block_or_udp(new_tcp_sock,contents,
				       filesize,udp_sock, root))==filesize) { 
	struct fileParts* newParts = malloc(sizeof(struct fileParts));
	//number of save commands sent to other servers
	int acks = putToServers(filename, contents, serv_port, root, newParts);
	//confirm that all the acks were received
	if (checkOks(serv_port, udp_sock, acks)){
	  //file was properly saved, create a new file index and add it to file list
	  struct fileList* newFile = malloc(sizeof(struct fileList));
	  strcpy(newFile->fileName, filename);
	  newFile->locations = newParts;
	  newFile->fileSize = filesize;
	  newFile->next = NULL;
	  updateFileList(listRoot, newFile);
	  ok(new_tcp_sock); 
	} else { 
	  free(newParts);
	  failed(new_tcp_sock); 
	} 
      } else { 
	flog("put: got only %d bytes of %d",bytesread,filesize); 
	failed(new_tcp_sock); 
      } 
      free(contents); 		/* done with buffer */ 
    } else if (req_is_del(request,filename)) { 
      flog("filename is '%s'",filename); 
      if (del(filename)) { 
	ok(new_tcp_sock); 
      } else { 
	failed(new_tcp_sock); 
      } 
    } else { 
      flog("unrecognized command"); 
      failed(new_tcp_sock); 
    } 
    close(new_tcp_sock);
    // print_all(); 
  }  
}

// In file included from squirrel.c:12:
// operations.h:3: warning: declaration of `socket' shadows a global declaration
// /usr/include/sys/socket.h:100: warning: shadowed declaration is here
// squirrel.c: In function `accept_or_udp':
// squirrel.c:47: warning: unused variable `retval'
// squirrel.c: In function `read_or_udp':
// squirrel.c:74: warning: implicit declaration of function `read'
// squirrel.c:66: warning: unused variable `retval'
// squirrel.c: At top level:
// squirrel.c:80: warning: declaration of `udp' shadows a global declaration
// operations.h:3: warning: shadowed declaration is here
// squirrel.c:92: warning: declaration of `udp' shadows a global declaration
// operations.h:3: warning: shadowed declaration is here
// squirrel.c: In function `ok':
// squirrel.c:158: warning: implicit declaration of function `write'
// squirrel.c: In function `main':
// squirrel.c:218: warning: implicit declaration of function `exit'
// squirrel.c:303: warning: implicit declaration of function `free'
// squirrel.c:309: warning: declaration of `contents' shadows a previous local
// squirrel.c:211: warning: shadowed declaration is here
// squirrel.c:309: warning: implicit declaration of function `malloc'
// squirrel.c:334: warning: implicit declaration of function `close'
