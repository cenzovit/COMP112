/*=============================
   help for Assignment 2: torrents 
  =============================*/ 

#include "help.h" 
#define TRUE  1
#define FALSE 0

/*=============================
   bit manipulation 
  =============================*/ 

void bits_alloc(struct bits *b, int nbits) { 
    b->nbits = nbits; 
    int size = BITSIZE(b); 
    b->array = (long *) malloc(size*sizeof(long)); 
    int i; 
    for (i=0; i<size; i++) b->array[i]=0; 
} 

void bits_free(struct bits *b) { 
    free(b->array); 
}

void bits_clearall(struct bits *b) { 
    int size = BITSIZE(b); int i; 
    for (i=0; i<size; i++) b->array[i]=0; 
} 
    
void bits_setrange(struct bits *b, int first, int last) { 
    int i; 
    if (first<0) first=0; 
    if (first>=b->nbits) first=b->nbits-1; 
    if (last<0) last=0; 
    if (last>=b->nbits) last=b->nbits-1; 
    int firstlong = first/BITSPERLONG;
    int lastlong = last/BITSPERLONG;
    int firstbit = first - firstlong*BITSPERLONG; 
    int lastbit  = last  - lastlong*BITSPERLONG; 
    if (firstlong != lastlong) { 
    /* low word */ 
        long lowmask; 
	if (firstbit==0) lowmask = -1L; 
	else lowmask = ((1L<<(BITSPERLONG-firstbit+1))-1)<<firstbit; 
	b->array[firstlong] |= lowmask; 
    /* middle words */ 
	for (i=firstlong+1; i<=lastlong-1; i++)
	    b->array[i]=-1L; /* 0xffffffffffffffff */
    /* high word */ 
	long highmask; 
        if (lastbit==BITSPERLONG-1) highmask=-1L; 
        else highmask = (1L<<(lastbit+1))-1; 
	b->array[lastlong] |= highmask; 
    } else { 
    /* inside one word */ 
        long midmask; 
	if (firstbit==0 && lastbit==BITSPERLONG-1) midmask=-1L; 
	else midmask = ((1L<<(lastbit-firstbit+1))-1)<<firstbit; 
	b->array[firstlong] |= midmask; 
    } 
} 

void bits_setbit(struct bits *b, int bit) { 
    if (bit<0 || bit>=b->nbits) { 
	return; 
    } 
    int whichbin = bit/BITSPERLONG; 
    int whichbit = bit%BITSPERLONG; 
    b->array[whichbin] |= 1L<<whichbit; 
} 

void bits_clearbit(struct bits *b, int bit) { 
    if (bit<0 || bit>=b->nbits) { 
	return; 
    } 
    int whichbin = bit/BITSPERLONG; 
    int whichbit = bit%BITSPERLONG; 
    b->array[whichbin] &= ~(1L<<whichbit); 
} 

int bits_testbit(struct bits *b, int bit) { 
    if (bit<0 || bit>=b->nbits) { 
	return FALSE; 
    } 
    int whichbin = bit/BITSPERLONG; 
    int whichbit = bit%BITSPERLONG; 
    return ((b->array[whichbin])&(1L<<whichbit))!=0; 
} 

int bits_empty(struct bits *b) { 
    int size = BITSIZE(b); int i; 
    for (i=0; i<size; i++) 
	if (b->array[i]) return FALSE; 
    return TRUE; 
    
} 

void bits_printall(struct bits *b) { 
    int size = BITSIZE(b); int i; 
    for (i=0; i<size; i++) { 
	fprintf(stderr, "%d: 0x%lx\n", i, b->array[i]); 
    } 
} 

void bits_printlist(struct bits *b) { 
    int i; 
    int first_one=TRUE; 
    for (i=0; i<b->nbits; i++) { 
	if (bits_testbit(b,i)) { 
	    if (first_one) { 
		first_one=FALSE; 
	    } else { 
		fprintf(stderr, ","); 
	    } 
	    fprintf(stderr, "%d", i); 
        } 
    } 
} 

/*=============================
   low-level protocol help 
  =============================*/ 

// translate a local binary structure to network order 
void block_local_to_network(struct block *local, struct block *net) { 
    net->which_block = htonl(local->which_block); 
    net->total_blocks = htonl(local->total_blocks); 
    net->paysize = htonl(local->paysize); 
    memcpy(net->filename, local->filename, FILENAMESIZE); 
    memcpy(net->payload, local->payload,   PAYLOADSIZE); 
} 

// translate a network binary structure to local order 
void block_network_to_local(struct block *local, struct block *net) { 
    local->which_block = ntohl(net->which_block); 
    local->total_blocks = ntohl(net->total_blocks); 
    local->paysize = ntohl(net->paysize); 
    memcpy(local->filename, net->filename,  FILENAMESIZE); 
    memcpy(local->payload, net->payload,  PAYLOADSIZE); 
} 
// translate a local binary structure to network order 
void command_local_to_network(struct command *local, struct command *network) {
    int i; 
    memcpy(network->filename, local->filename, FILENAMESIZE); 
    network->nranges = htonl(local->nranges); 
    for (i=0; i<MAXRANGES; i++) { 
	network->ranges[i].first_block = htonl(local->ranges[i].first_block); 
	network->ranges[i].last_block = htonl(local->ranges[i].last_block); 
    } 
} 

// translate a network binary structure to local order 
void command_network_to_local(struct command *local, struct command *network) {
    int i; 
    memcpy(local->filename, network->filename,  FILENAMESIZE); 
    local->nranges = ntohl(network->nranges); 
    for (i=0; i<MAXRANGES; i++) { 
	local->ranges[i].first_block = ntohl(network->ranges[i].first_block); 
	local->ranges[i].last_block = ntohl(network->ranges[i].last_block); 
    } 
} 

/*=============================
   high-level protocol help 
  =============================*/ 

/* send a command to a server */ 
int send_command(int sockfd, const char *address, int port, 
	  	 const char *filename, int first, int last) { 
    struct command loc_cmd, net_cmd; 
    struct sockaddr_in server_addr; 
/* set up cmd structure */ 
    memset(&loc_cmd, 0, sizeof(loc_cmd)); 
    strcpy(loc_cmd.filename, filename); 
    loc_cmd.nranges=1; 
    loc_cmd.ranges[0].first_block = first; 
    loc_cmd.ranges[0].last_block = last; 
/* translate to network order */ 
    command_local_to_network(&loc_cmd, &net_cmd) ; 
/* set up target address */ 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = PF_INET;
    inet_pton(PF_INET, address, &server_addr.sin_addr);
    server_addr.sin_port        = htons(port);
/* send the packet: only send instantiated ranges */ 
    int sendsize = COMMAND_SIZE(loc_cmd.nranges);
    int ret = sendto(sockfd, (void *)&net_cmd, sendsize, 0, 
	(struct sockaddr *)&server_addr, sizeof(server_addr)); 
    if (ret<0) perror("send_command"); 
    return ret;
}

/* send multiple commands to a server */ 
int send_commands(int sockfd, const char *address, int port, 
		  const char *filename, struct range ranges[], int num) { 
    struct command loc_cmd, net_cmd; 
    struct sockaddr_in server_addr; 
/* set up cmd structure */ 
    memset(&loc_cmd, 0, sizeof(loc_cmd)); 
    strcpy(loc_cmd.filename, filename); 
    loc_cmd.nranges=num;
    int i;
    for(i = 0; i < num; i++){
      loc_cmd.ranges[i].first_block = ranges[i].first_block; 
      loc_cmd.ranges[i].last_block = ranges[i].last_block; 
    }
    
/* translate to network order */ 
    command_local_to_network(&loc_cmd, &net_cmd) ; 
/* set up target address */ 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = PF_INET;
    inet_pton(PF_INET, address, &server_addr.sin_addr);
    server_addr.sin_port        = htons(port);
/* send the packet: only send instantiated ranges */ 
    int sendsize = COMMAND_SIZE(loc_cmd.nranges);
    int ret = sendto(sockfd, (void *)&net_cmd, sendsize, 0, 
	(struct sockaddr *)&server_addr, sizeof(server_addr)); 
    if (ret<0) perror("send_command"); 
    return ret;
} 

/* receive a block from a server */ 
int recv_block(int sockfd, struct block *b, struct sockaddr_in *resp_addr) { 
    int resp_len; 			/* length used */ 
    int resp_mesglen; 
    struct block net_blk; 
again: 
    resp_mesglen = 0; 
    resp_len = sizeof(resp_addr); /* MUST initialize this */ 
    resp_mesglen = recvfrom(sockfd, &net_blk, sizeof(net_blk), 0, 
	    (struct sockaddr *) resp_addr, &resp_len);
    /* check for socket error */ 
    if (resp_mesglen<0) { 
	perror("recv_block"); 
	return resp_mesglen; 
    /* check for erronious packets */ 
    } else if (resp_mesglen!=sizeof(net_blk)) { 
	fprintf(stderr, "recv_block: received %d bytes, expected %d", 
		resp_mesglen, sizeof(struct block)); 
        fprintf(stderr, " -- ignoring bad input\n"); 
	goto again; 
    } 
    /* translate to local byte order */ 
    block_network_to_local(b, &net_blk); 
    return resp_mesglen; 
} 

/* run a "select" on the local socket, with timeout */ 
int select_block(int sockfd, int seconds, int microsec) { 
  /* set up a timeout wait for input */ 
    struct timeval tv;
    fd_set rfds;
    int retval;
  /* Watch sockfd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
  /* Wait up to one second. */
    tv.tv_sec = seconds; tv.tv_usec = microsec;
    return select(sockfd+1, &rfds, NULL, NULL, &tv); 
} 

/*=============================
  client usage and argument checking 
  =============================*/
void client_usage() { 
    fprintf(stderr,
	"client usage: ./client {server_address} {server_port} {client_port} {filename}\n");
    fprintf(stderr, 
	"  {server_address} is the IP address where the server is running.\n"); 
    fprintf(stderr, 
	"  {server_port} is the port number of the server.\n"); 
    fprintf(stderr, 
	"  {client_port} is the port this client should use.\n"); 
    fprintf(stderr, 
	"  {filename} is the file name to get.\n"); 
} 

int client_arguments_valid(const char *server_dotted, int server_port, 
			   int client_port, const char *filename) { 

    int valid = TRUE; 
    struct in_addr server_addr; 	/* native address */ 
    if (inet_pton(AF_INET, server_dotted, &server_addr)<=0) { 
	fprintf(stderr, "client: server address %s is not valid\n", 
		server_dotted); 
	valid=FALSE; 
	
    } 
    
    if (server_port<9000 || server_port>32767) { 
	fprintf(stderr, "client: server_port %d is not allowed\n", 
		server_port); 
	valid=FALSE; 
    } 

    if (client_port<9000 || client_port>32767) { 
	fprintf(stderr, "client: client_port %d is not allowed\n", 
		client_port); 
	valid=FALSE; 
    } 

    if (index(filename, '/')) { 
	fprintf(stderr, "client: '/' is not allowed in filename %s\n", 
		filename); 
	valid=FALSE; 
    } 
    return valid; 
} 

/*=============================
  routines that only work properly in Halligan hall
  (because they understand Halligan network policies)
  read the primary IP address for an ECE/CS linux host 
  this is always the address bound to interface eth0
  this is used to avoid listening (by default) on 
  secondary interfaces. 
  =============================*/ 
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
