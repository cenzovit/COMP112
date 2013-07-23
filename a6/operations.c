#include <stdio.h> 
#include <stdarg.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <stdlib.h> 
#include <unistd.h> 

#include "storage.h"
#include "operations.h"

#define TRUE 1
#define FALSE 0

/* logging of server actions */ 
#define MAXOUT 256 		/* maximum number of output chars for flog */ 
static void flog(const char *fmt, ...) {
    va_list ap;
    char *p; 
    char buf[MAXOUT]; 
    va_start(ap, fmt);
    fprintf(stderr,"[operations: "); 
    vsnprintf(buf,MAXOUT,fmt,ap); 
    for (p=buf; *p && p-buf<MAXOUT; p++) 
	if ((*p)>=' ') 
	    putc(*p,stderr); 
    fprintf(stderr,"]\n"); 
    va_end(ap);
} 

//Function used to update live server list 
void updateList(struct index* root, char* cli_dotted){ 
  struct index *temp = root;
  for( ; ; ){
    //Check if IP is already in list
    if(temp->next != NULL){
      if(strcmp(temp->next->ipa, cli_dotted) == 0){
	break;
      }
      else{
	temp = temp->next;
      }
    }
    else{
      struct index *newIndex = malloc(sizeof(struct index));
      strcpy(newIndex->ipa, cli_dotted);
      newIndex->next = NULL;
      temp->next = newIndex;
      break;
    }
  } 
}

//Function used to update the saved file list
void updateFileList(struct fileList* root, struct fileList* newFile){
  struct fileList *temp = root;
  for( ; ; ){
    if(temp->next != NULL){
      temp = temp->next;
    }
    else{
      temp->next = newFile;
      break;
    }
  } 
}

//function to send a message to a certain server
//  ie respond to a server's message
void sendToServer(char* ip, int port, char* payload){
  int sockfd;
  char *send_line = malloc(strlen(payload));
  struct sockaddr_in recv_addr;

  /* set target address */ 
  memset(&recv_addr, 0, sizeof(recv_addr));
  recv_addr.sin_family = PF_INET;
  inet_pton(PF_INET, ip, &recv_addr.sin_addr);
  recv_addr.sin_port = htons(port);
  strcpy(send_line, payload);
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  
  if (sendto(sockfd, send_line, strlen(send_line), 0, 
	     (struct sockaddr *)&recv_addr, sizeof(recv_addr))==(-1)){
    perror("sendto"); 
  }
  free(send_line);
}

//special "udp" function to count acks recieved
int checkOks(int port, int udpsock, int acks){
  int counter = 0;
  time_t okTimer;

  fd_set rfds;

  startTimer(&okTimer);  
  while(checkTimer(&okTimer, 10) == 0 && counter != acks){
     // Set up to use select() as a timeout for listening
    struct timeval stTimeOut;
    
    // Timeout of one second
    stTimeOut.tv_sec = 1;
    stTimeOut.tv_usec = 0;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds); 
    FD_SET(udpsock, &rfds); 
    
    int s = select((udpsock)+1, &rfds, NULL, NULL, &stTimeOut);
    
    // If select = -1, there was an error
    if(s == -1){
      fprintf(stderr, "Call to select() failed!");
    }
    // If select = 1, there was something to be read
    else if (s == 1)  {
      if (FD_ISSET(udpsock,&rfds)){
	counter = counter + redundancyCheck(udpsock);
      }
    } 
    // Select = 0, timeout reached
    else{
    }
  }
  if(counter == acks){
    return TRUE;
  }
  else{
    return FALSE;
  }
}

void waitForPings(int udpsock, struct index* root){
  time_t okTimer;

  fd_set rfds;

  startTimer(&okTimer);  
  while(checkTimer(&okTimer, 2) == 0){
     // Set up to use select() as a timeout for listening
    struct timeval stTimeOut;
    
    // Timeout of one second
    stTimeOut.tv_sec = 1;
    stTimeOut.tv_usec = 0;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds); 
    FD_SET(udpsock, &rfds); 
    
    int s = select((udpsock)+1, &rfds, NULL, NULL, &stTimeOut);
    
    // If select = -1, there was an error
    if(s == -1){
      fprintf(stderr, "Call to select() failed!");
    }
    // If select = 1, there was something to be read
    else if (s == 1)  {
      if (FD_ISSET(udpsock,&rfds)){
	udp(udpsock, root);
      }
    } 
    // Select = 0, timeout reached
    else{
    }
  }
}

int putToServers(char *name, char *content, int port, struct index* root, struct fileParts* parts){
  int servers = numServers(root);
  int distributionSize = strlen(content)/servers; 
  int leftover = strlen(content)%servers;
  int conductor = 0;
  int blockNum = 0;
  struct index* temp = root->next;
  struct fileParts* tempParts = parts;

  while(conductor < strlen(content)){
    int nestedConductor = 0;
    char blockString[10];
    sprintf(blockString, "%d", blockNum);
    //While you still need to add the extra bits to certain servers
    if(leftover > 0){
      char contentPiece[distributionSize+1];
      //Cut the message into the correct piece
      while(nestedConductor<distributionSize+1){
	contentPiece[nestedConductor] = content[conductor];
	nestedConductor++;
	conductor++;
      }
      contentPiece[distributionSize+1] = '\0';
      char* message = malloc(strlen("save-")+strlen(name)+strlen("-")+
			     strlen(contentPiece)+strlen(blockString));
      strcpy(message, "save-");
      strcat(message, name);
      strcat(message, blockString);
      strcat(message, "-");
      strcat(message, contentPiece);
      message[strlen(message)] = '\0';
      //if you've reached the end of the linked list, start over from
      // the beginning
      if(temp->next == NULL){
	sendToServer(temp->ipa, port, message);
	sendToServer(root->next->ipa, port, message);
	struct fileParts* newPart = malloc(sizeof(struct fileParts));
	strcpy(newPart->locationOne, temp->ipa);
	strcpy(newPart->locationTwo, root->next->ipa);
	newPart->blockNum = blockNum;
	newPart->next = NULL;
	tempParts->next = newPart;
	tempParts = tempParts->next;
	temp = root->next;
      }
      else{
	sendToServer(temp->ipa, port, message);
	sendToServer(temp->next->ipa, port, message);
	struct fileParts* newPart = malloc(sizeof(struct fileParts));
	strcpy(newPart->locationOne, temp->ipa);
	strcpy(newPart->locationTwo, temp->next->ipa);
	newPart->blockNum = blockNum;
	newPart->next = NULL;
	tempParts->next = newPart;
	tempParts = tempParts->next;
	temp = temp->next;
      }
      free(message);
      blockNum = blockNum + 1;
      leftover--;
    }
    else{
      char contentPiece[distributionSize];
      while(nestedConductor<distributionSize){
	contentPiece[nestedConductor] = content[conductor];
	nestedConductor++;
	conductor++;
      }
      contentPiece[distributionSize] = '\0';
      char* message = malloc(strlen("save-")+strlen(name)+strlen("-")+
			     strlen(contentPiece)+strlen(blockString));
      strcpy(message, "save-");
      strcat(message, name);
      strcat(message, blockString);
      strcat(message, "-");
      strcat(message, contentPiece);
      message[strlen(message)] = '\0';
      if(temp->next == NULL){
	sendToServer(temp->ipa, port, message);
	sendToServer(root->next->ipa, port, message);
	struct fileParts* newPart = malloc(sizeof(struct fileParts));
	strcpy(newPart->locationOne, temp->ipa);
	strcpy(newPart->locationTwo, root->next->ipa);
	newPart->blockNum = blockNum;
	newPart->next = NULL;
	tempParts->next = newPart;
	tempParts = tempParts->next;
	temp = root->next;
      }
      else{
	sendToServer(temp->ipa, port, message);
	sendToServer(temp->next->ipa, port, message);
	struct fileParts* newPart = malloc(sizeof(struct fileParts));
	strcpy(newPart->locationOne, temp->ipa);
	strcpy(newPart->locationTwo, temp->next->ipa);
	newPart->blockNum = blockNum;
	newPart->next = NULL;
	tempParts->next = newPart;
	tempParts = tempParts->next;
	temp = temp->next;
      }
      blockNum = blockNum + 1;
    }
  }
  return servers*2;
}


int getPartFromServers(int udpsock, char** content, int port, int block, struct index* root){
 
  time_t okTimer;

  fd_set rfds;

  startTimer(&okTimer);  
  //wait only 2 seconds for the return message from the server
  while(checkTimer(&okTimer, 2) == 0){
     // Set up to use select() as a timeout for listening
    struct timeval stTimeOut;
    
    // Timeout of one second
    stTimeOut.tv_sec = 1;
    stTimeOut.tv_usec = 0;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds); 
    FD_SET(udpsock, &rfds); 
    
    int s = select((udpsock)+1, &rfds, NULL, NULL, &stTimeOut);
    
    // If select = -1, there was an error
    if(s == -1){
      fprintf(stderr, "Call to select() failed!");
    }
    // If select = 1, there was something to be read
    else if (s == 1)  {
      if (FD_ISSET(udpsock,&rfds)){
	if(udpGetPart(udpsock, content, block, root)){
	  return TRUE;
	}
      }
    } 
    // Select = 0, timeout reached
    else{
    }
  }
  return FALSE;
}

int getFromServers(char* name, char** content, int* size, struct index* rootTwo,struct fileList* root, int udpsock, int port){
  struct fileList* temp = root;
  while(temp->next != NULL){
    if(strcmp(temp->next->fileName, name) == 0){
      temp = temp->next;
      break;
    }
    temp = temp->next;
  }
  if(strcmp(temp->fileName, name) == 0){
    struct fileParts* tempParts = temp->locations->next;
    *size = temp->fileSize;
    *content = (char *)malloc(*size);
    int blockNum = tempParts->blockNum;
    char blockString[10];
    char* message;
    while(TRUE){
      sprintf(blockString, "%d", blockNum);
      message = malloc(strlen("get-")+strlen(name)+strlen(blockString));
      strcpy(message, "get-");
      strcat(message, name);
      strcat(message, blockString);
      sendToServer(tempParts->locationOne, serv_port, message);
      
      if(getPartFromServers(udpsock, content, port, blockNum, rootTwo)){
	tempParts = tempParts->next;
	blockNum = blockNum + 1;
      }
      else{
	sendToServer(tempParts->locationTwo, serv_port, message);
	if(getPartFromServers(udpsock, content, port, blockNum, rootTwo)){
	  tempParts = tempParts->next;
	  blockNum = blockNum + 1;
	}
	else{
	  
	  free(message);
	  return FALSE;
	}
      }
      if(tempParts == NULL){
	free(message);
	return TRUE;
      }
      free(message);
    }
  }
  else{
    return FALSE;
  }
}

int udpGetPart(int sockfd, char** contents, int block, struct index* root){
  /* client data */
  struct sockaddr_in cli_addr;        /* raw client address */
  int cli_len;                        /* length used */
  char cli_dotted[MAXADDR];           /* string ip address */
  in_addr_t cli_ulong;            /* packed ip address */
  struct hostent *cli_hostent;        /* host entry */
  /* message parameters */ 
  char message[MAXMESG]; 		/* message to be read */ 
  int mesglen=0; 			/* message length */ 
  
  flog("udp datagram available on socket %d\n",sockfd); 
  
  /* get a datagram */
  cli_len = sizeof(cli_addr); /* MUST initialize this */
  mesglen = recvfrom(sockfd, message, MAXMESG, 0,
		     (struct sockaddr *) &cli_addr, &cli_len);
  /* get numeric internet address */
  inet_ntop(PF_INET, (void *)&(cli_addr.sin_addr.s_addr),
	    cli_dotted, MAXADDR);
  flog("udp connection from %s\n",cli_dotted);
    
  /* convert numeric internet address to name */
  cli_ulong = cli_addr.sin_addr.s_addr;
  cli_hostent = gethostbyaddr((char *)&cli_ulong, 
			      sizeof(cli_ulong),PF_INET);
  if (cli_hostent) {
    flog("udp host name is %s\n", cli_hostent->h_name);
  } else {
    flog("no name for udp host\n");
  }
  message[mesglen]='\0'; // moot point; makes it a string if possible
  //flog("message is '%s'",message);
  char* command;
  char* filename;
  char* content;
  
  command = strtok(message, "-");
  filename = strtok(NULL, "-");
  content = strtok(NULL, "-");

  if(strcmp(cli_hostent->h_name, "localhost") == 0){
    flog("ignoring message from localhost");
  }
  else if(strcmp(message, "retrieved") == 0){
    flog("message is '%s'",command);
    if(block == 0){
      strcpy(*contents, content);
    }
    else{
      strcat(*contents, content);
    }
    return 1;
  }
  else if(strcmp(command, "get") == 0){
    flog("message is '%s'",command);

    char* actualName = (char*)malloc(strlen(filename)+strlen(cli_dotted));
    strcpy(actualName, filename);
    strcat(actualName, cli_dotted);
    int temp;
    char* returned;
    if(get(actualName, &returned, &temp)){
      char* sendCommand = malloc(strlen("retrieved-")+strlen(filename)+strlen(returned)+strlen("-"));
      strcpy(sendCommand, "retrieved-");
      strcat(sendCommand, filename);
      strcat(sendCommand, "-");
      strcat(sendCommand, returned); 
      sendToServer(cli_dotted, serv_port, sendCommand);
    }
    free(actualName);
  }
  else{
    flog("message is not an 'retrieved' or 'get', ignore.");
  }
  return 0;
}

int redundancyCheck(int sockfd){
  /* client data */
  struct sockaddr_in cli_addr;        /* raw client address */
  int cli_len;                        /* length used */
  char cli_dotted[MAXADDR];           /* string ip address */
  in_addr_t cli_ulong;            /* packed ip address */
  struct hostent *cli_hostent;        /* host entry */
  /* message parameters */ 
  char message[MAXMESG];       	/* message to be read */ 
  int mesglen=0; 			/* message length */ 
  
  flog("udp datagram available on socket %d\n",sockfd); 
  
  /* get a datagram */
  cli_len = sizeof(cli_addr); /* MUST initialize this */
  mesglen = recvfrom(sockfd, message, MAXMESG, 0,
		     (struct sockaddr *) &cli_addr, &cli_len);
  /* get numeric internet address */
  inet_ntop(PF_INET, (void *)&(cli_addr.sin_addr.s_addr),
	    cli_dotted, MAXADDR);
  flog("udp connection from %s\n",cli_dotted);
    
  /* convert numeric internet address to name */
  cli_ulong = cli_addr.sin_addr.s_addr;
  cli_hostent = gethostbyaddr((char *)&cli_ulong, 
			      sizeof(cli_ulong),PF_INET);
  
  if (cli_hostent->h_name != NULL) {
    flog("udp host name is %s\n", cli_hostent->h_name);
  } else {
    flog("no name for udp host\n");
  }

  message[mesglen]='\0'; // moot point; makes it a string if possible
  
  char* command;
  char* filename;
  char* content;
  
  command = strtok(message, "-");
  filename = strtok(NULL, "-");
  content = strtok(NULL, "-");

  fprintf(stderr, "command: %s\n", command);
  fprintf(stderr, "filename: %s\n", filename);
  fprintf(stderr, "content: %s\n", content);
 
  if(strcmp(cli_hostent->h_name, "localhost") == 0){
    flog("ignoring message from localhost");
  }
  else if(strcmp(message, "ok") == 0){
    flog("message is '%s'",command);
    return 1;
  }
  else if(strcmp(command, "save") == 0){
    flog("message is '%s'",command);
    char* newName = malloc(strlen(filename)+strlen(cli_dotted));
    strcpy(newName, filename);
    strcat(newName, cli_dotted);
    if(put(newName, content, strlen(content))){
      sendToServer(cli_dotted, serv_port, "ok");
    }
    else{
      sendToServer(cli_dotted, serv_port, "failed");
    }
    free(newName);
  }
  else{
    flog("message is not an 'ok' or 'save', ignore.");
  }
  return 0;
}

/* called when udp datagram available on a socket 
 * socket: number of socket */ 
void udp(int sockfd, struct index* root) { 
  /* client data */
  struct sockaddr_in cli_addr;        /* raw client address */
  int cli_len;                        /* length used */
  char cli_dotted[MAXADDR];           /* string ip address */
  in_addr_t cli_ulong;            /* packed ip address */
  struct hostent *cli_hostent;        /* host entry */
  /* message parameters */ 
  char message[MAXMESG];       	/* message to be read */ 
  int mesglen=0; 			/* message length */ 
  
  flog("udp datagram available on socket %d\n",sockfd); 
  
  /* get a datagram */
  cli_len = sizeof(cli_addr); /* MUST initialize this */
  mesglen = recvfrom(sockfd, message, MAXMESG, 0,
		     (struct sockaddr *) &cli_addr, &cli_len);
  /* get numeric internet address */
  inet_ntop(PF_INET, (void *)&(cli_addr.sin_addr.s_addr),
	    cli_dotted, MAXADDR);
  flog("udp connection from %s\n",cli_dotted);
    
  /* convert numeric internet address to name */
  cli_ulong = cli_addr.sin_addr.s_addr;
  cli_hostent = gethostbyaddr((char *)&cli_ulong, 
			      sizeof(cli_ulong),PF_INET);
  
  if (cli_hostent) {
    flog("udp host name is %s\n", cli_hostent->h_name);
  } else {
    flog("no name for udp host\n");
  }

  message[mesglen]='\0'; // moot point; makes it a string if possible
  
  char* command;
  char* filename;
  char* content;
  
  
  command = strtok(message, "-");
  filename = strtok(NULL, "-");
  content = strtok(NULL, "-");

  flog("message is '%s'",command); 
  if(strcmp(cli_hostent->h_name, "localhost") == 0){
    flog("ignoring message from localhost");
  }
  else if(strcmp(command, "ping") == 0){
    updateList(root, cli_dotted); 
  }  
  else if(strcmp(command, "save") == 0){
    char* newName = malloc(strlen(filename)+strlen(cli_dotted));
    strcpy(newName, filename);
    strcat(newName, cli_dotted);
    if(put(newName, content, strlen(content))){
      sendToServer(cli_dotted, serv_port, "ok");
    }
    else{
      sendToServer(cli_dotted, serv_port, "failed");
    }
  }
  else if(strcmp(command, "get") == 0){
    flog("message is '%s'",command);

    char* actualName = (char*)malloc(strlen(filename)+strlen(cli_dotted));
    strcpy(actualName, filename);
    strcat(actualName, cli_dotted);
    int temp;
    char* returned;
    if(get(actualName, &returned, &temp)){
      char* sendCommand = malloc(strlen("retrieved-")+strlen(filename)+strlen(returned)+strlen("-"));
      strcpy(sendCommand, "retrieved-");
      strcat(sendCommand, filename);
      strcat(sendCommand, "-");
      strcat(sendCommand, returned); 
      sendToServer(cli_dotted, serv_port, sendCommand);
    }
    free(actualName);
  }
  else if(strcmp(command, "delete") == 0){
    flog("message is '%s'",command);
    char* actualName = (char*)malloc(strlen(filename)+strlen(cli_dotted));
    strcpy(actualName, filename);
    strcat(actualName, cli_dotted);
    del(actualName);
    free(actualName);
  }
}
 

/* get a file from storage;
 * name: name of file (in local machine)
 * content: an array of content (result parameter)
 * size: size of file */ 
int get(char *name, char **content, int *size) { 
    if (get_size(name)>=0) {  
	int blocks; 
	*size=get_size(name); 
	blocks = (*size%BLOCKSIZE==0?*size/BLOCKSIZE:*size/BLOCKSIZE+1); 
	if (blocks>0) { 
	    *content= (char *)malloc(BLOCKSIZE*blocks); 
	    int i;
	    for (i=0; i<blocks; i++) 
		get_fblock(name,i,*content+i*BLOCKSIZE); 
        } else { 
	    *content = (char *)malloc(1*sizeof(char)); 
	    **content=0; 
	} 
	return TRUE; 
    } else { 
	return FALSE; 
    } 
} 

/* put a file into storage. 
 * name: local name of file. 
 * content: a character array of content. 
 * size: size of local file. */ 
int put(char *name, char *content, int size) { 
    int blocks; 
    remember_size(name,size); 
    blocks = (size%BLOCKSIZE==0?size/BLOCKSIZE:size/BLOCKSIZE+1); 
    if (blocks>0) { 
	int i; 
	for (i=0; i<blocks; i++) { 
	    int n = next_cblock(); 
	    if (n>=0) { 
		remember_fblock(name,i,content+i*BLOCKSIZE); 
	    } else { 
		flog("no more cache!\n"); 
		return FALSE; 
	    } 
	} 
    } 
    return TRUE; 
} 

/* delete a file from storage 
 * name: name of file (on one machine) */ 
int del(char *name)  {
    if ((get_size(name))>=0) { 
	forget_size(name); 
	forget_file(name);
	return TRUE; 
    } else { 
	return FALSE; 
    } 
} 


