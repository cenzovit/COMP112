//Vincenzo Vitiello
//COMP112 - a5
//Prof. Couch

#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <time.h>

#define MAX_MESG 80
#define MAX_ADDR 80
#define SERV_UDP_PORT 8005

/* ******** From halligan.C ******** */

// read the primary IP address for an ECE/CS host 
// this is always the address bound to interface eth0
// this is used to avoid listening (by default) on 
// maintenance interfaces. 
int get_primary_addr(struct in_addr *a) { 
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    if (!getifaddrs(&ifAddrStruct)) // get linked list of interface specs
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa ->ifa_addr->sa_family==AF_INET) {  // is an IP4 Address
		if (strcmp(ifa->ifa_name,"eth0")==0) { // is for interface eth0
		    void *tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)
			->sin_addr;
		    memcpy(a, tmpAddrPtr, sizeof(struct in_addr)); 
		    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
		    return 0; // found 
		} 
	    } 
	}
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return -1; // not found
} 

/* ********************************* */

//Struct for my ip address linked list (a node)
struct index{
  char ipa[MAX_ADDR];
  struct index* next;
};

//Function to print IPs from the linked list and then free the mem
// that was malloc'd for the nodes
void printAndFreeList(struct index *index){
  if(index->next != NULL){
    printAndFreeList(index->next);
  }
  fprintf(stderr, "IP: %s\n", index->ipa);
  free(index);
}

//Fuction to create a "timer"
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

//Taken from send-broadcast.c - rewritten as a function
void sendBroadcast(char* port){
  int sockfd;
  struct sockaddr_in recv_addr;
  char *send_line = NULL; 
  
  /* set target address */ 
  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family = PF_INET;
  recv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  // inet_pton(PF_INET, "130.64.23.255", &recv_addr.sin_addr);
  recv_addr.sin_port = htons(atoi(port));
  send_line = "-HELLO:WORLD-"; 
  
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

int main(int argc, char **argv){
  time_t fiveSecTimer;
  time_t twoSecTimer;
  int sockfd;

  //init linked list by creating a root handle
  struct index *root;
  root = malloc(sizeof(struct index));
  strcpy(root->ipa, ""); 
  root->next = NULL;
    
  /* receiver data */ 
  struct sockaddr_in recv_addr; 	/* server address */ 
  /* message data */ 
  int mesglen; char message[MAX_MESG];
  /* sender data */ 
  struct sockaddr_in send_addr; 	/* raw sender address */ 
  int send_len; 			/* length used */ 
  char send_dotted[MAX_ADDR]; 	/* string ip address */ 
  int recv_port = 0; 
  
  if (argc != 2) { 
    fprintf(stderr, "%s usage: %s port\n", argv[0], argv[0]); 
    exit(1);
  } 
  char* port = argv[1];
  recv_port=atoi(port); 
  if (recv_port<9000 || recv_port>32767) { 
    fprintf(stderr, "%s: port %d is not allowed\n", argv[0], recv_port); 
    exit(1); 
  } 
  /* get the primary IP address of this host */ 
  struct in_addr primary; 
  get_primary_addr(&primary); 
  char primary_dotted[INET_ADDRSTRLEN]; 
  inet_ntop(AF_INET, &primary, primary_dotted, INET_ADDRSTRLEN);
  fprintf(stderr, "%s: Running on %s, port %d\n\n", argv[0],
	  primary_dotted, recv_port); 
  
  /* make a socket*/ 
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  
  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family      = PF_INET;
  // memcpy(&recv_addr.sin_addr,&primary,sizeof(struct in_addr)); 
  // must use INADDR_ANY to receive broadcasts
  recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  recv_addr.sin_port        = htons(recv_port);
  
  /* bind it to the primary address and selected port on this host */ 
  if (bind(sockfd, (struct sockaddr *) &recv_addr, sizeof(recv_addr))<0) 
    perror("bind"); 
  
  /* allow broadcasts */ 
  const int broadcast = 1;
  if((setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,
		 &broadcast,sizeof broadcast))) {
    perror("setsockopt"); exit(1); 
  } 

  sendBroadcast(port);
  startTimer(&twoSecTimer);
  startTimer(&fiveSecTimer);
  for ( ; ; ) {
    // Set up to use select() as a timeout for listening
    struct timeval stTimeOut;
    fd_set stReadFDS;
    FD_ZERO(&stReadFDS);
    
    // Timeout of one second
    stTimeOut.tv_sec = 1;
    stTimeOut.tv_usec = 0;
    
    FD_SET(sockfd, &stReadFDS);
    
    //Use select function to timeout the recvfrom function
    // I.E. check if there is something to read before calling
    // recvfrom
    int t = select(sockfd+1, &stReadFDS, NULL, NULL, &stTimeOut);
    //If select = -1, there was an error
    if (t == -1) {
      fprintf(stderr, "Call to select() failed");
    }
    //If select = 1, there is something to be read
    else if (t == 1) {
      if (FD_ISSET(sockfd, &stReadFDS)) {
	/* get a datagram */ 
	send_len = sizeof(send_addr); /* MUST initialize this */ 
	mesglen = recvfrom(sockfd, message, MAX_MESG, 0, 
			   (struct sockaddr *) &send_addr, &send_len);
	
	if (mesglen>=0) { 
	  /* get numeric internet address */
	  inet_ntop(PF_INET, (void *)&(send_addr.sin_addr.s_addr),  
		    send_dotted, MAX_ADDR);
	  //fprintf(stderr, "server: connection from %s\n",send_dotted);
	  message[mesglen]='\0'; 
	  //fprintf(stderr, "server received: %s\n",message); 
	  struct index *newIndex = malloc(sizeof(struct index));
	  strcpy(newIndex->ipa, send_dotted);
	  newIndex->next = NULL;
	  struct index *temp = root;
	  for( ; ; ){
	    //Check if IP is already in list
	    if(temp->next != NULL){
	      if(strcmp(temp->next->ipa, newIndex->ipa) == 0){
		free(newIndex);
		break;
	      }
	      else{
		temp = temp->next;
	      }
	    }
	    else{
	      temp->next = newIndex;
	      break;
	    }
	  }
	} 
	else{ 
	  perror("receive failed"); 
	} 
      }
    }
    //If select = 0, timeout was reached
    else {
      //timeout reached
    }
    //If 2 seconds have passed, re-broadcast that you are active
    if(checkTimer(&twoSecTimer, 2) == 1){
      sendBroadcast(port);
    }
    //If 5 seconds have passed, print the list of IPs and reset the list
    if(checkTimer(&fiveSecTimer, 5) == 1){
      fprintf(stderr, "Current Active Servers:\n");
      if(root->next != NULL){
	printAndFreeList(root->next);
	root->next = NULL;
      }
      fprintf(stderr, "\n");
    }   
  }
  free(root);
}
