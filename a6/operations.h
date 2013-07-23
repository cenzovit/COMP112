#define SERV_PORT 8000 	/* server's port number */
#define SERV_BLOCKS 100		/* number of blocks to buffer */ 

#define MAX_MESG 80
#define MAX_ADDR 80
#define SERV_UDP_PORT 8005
#define MAX_NAME 256

#define TRUE 1
#define FALSE 0

int serv_port;
//int toggle = 0;

//Data Structure(linked-list) for List of active servers
struct index{
  char ipa[MAX_ADDR];
  struct index* next;
};

struct fileParts{
  char locationOne[MAX_ADDR];
  char locationTwo[MAX_ADDR];
  int blockNum;
  struct fileParts* next;
};

struct fileList{
  char fileName[MAX_NAME];
  int fileSize;
  struct fileParts* locations;
  struct fileList* next;
};



/* called when udp datagram available on a socket 
 * socket: number of socket */ 
void udp(int sock, struct index* root); 

/* get a file from storage;
 * name: name of file (in local machine)
 * content: an array of content (result parameter)
 * size: size of file */ 
int get(char *name, char **content, int *s); 

/* put a file into storage. 
 * name: local name of file. 
 * content: a character array of content. 
 * size: size of local file. */ 
int put(char *name, char *content, int size); 

/* delete a file from storage 
 * name: name of file (on one machine) */ 
int del(char *name); 

//function which will add the new IP address to the linked list of IPs
void updateList(struct index* root, char* cli_dotted);

//function which will add the newFile to the linked list of saved files
void updateFileList(struct fileList* root, struct fileList* newFile);

//function which sends payload to the server at IP
void sendToServer(char* ip, int port, char* payload);

//function which listens to UDP for 'save' or 'ok' when confirming that 
//  the other servers have properly saved the file pieces
int redundancyCheck(int sockfd);

//function which listens to UDP for 'retrieved' or 'get' when retrieving
//  the pieces of the file and combining them back into one
int udpGetPart(int sockfd, char** contents, int block, struct index* root);

//function which recombines the redundantly saved file by asking the correct
//  servers for the pieces they had been asked to save
int getFromServers(char* name, char** content, int* size, struct index* rootTwo,struct fileList* root, int udpsock, int port);

//function which waits on UDP to confirm that a specific piece of the file has been
//  properly returned
int getPartFromServers(int udpsock, char** content, int port, int block, struct index* root);

//function which redundantly distrubutes the file based upon what servers are currently
//  active
int putToServers(char *name, char *content, int port, struct index* root, struct fileParts* parts);

//function which confirms that all acks have been recieved (IE the file has been stored redundantly)
int checkOks(int port, int udpsock, int acks);

//function which gives the server a moment to collect pings and generate an ip list
void waitForPings(int udpsock, struct index* root);
