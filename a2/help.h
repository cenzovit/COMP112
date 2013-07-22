#ifndef __HELP_H__
#define __HELP_H__ 1

/*=============================
  helper routines for Assignment 2: torrents 
  =============================*/ 

#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <values.h> 
#include <fcntl.h> 

/*=============================
  protocol definitions 
  =============================*/ 

#define PAYLOADSIZE 128		/* file bytes per packet */ 
#define FILENAMESIZE 48		/* limit on file name size */ 
#define MAXRANGES    12		/* ranges in one command */ 

// a range of blocks to send 
struct range { 
    uint32_t first_block; 
    uint32_t last_block; 
} ; 

// a response from the server: a piece of a file 
struct block { 
    char filename[FILENAMESIZE]; 
    uint32_t which_block; 
    uint32_t total_blocks; 
    uint32_t paysize; 
    char payload[PAYLOADSIZE]; 
} ; 

// command to the server : which blocks of what file to get
struct command { 
    char filename[FILENAMESIZE]; 
    uint32_t nranges; 
    struct range ranges[MAXRANGES]; 
} ; 

// this is used in size calculations i
// to send partial structs
struct command_prefix { 
    char filename[FILENAMESIZE]; 
    uint32_t nranges; 
    struct range ranges[1]; 
} ; 

// compute the size of a command to send or receive
#define COMMAND_SIZE(RANGES) \
    (sizeof(struct command_prefix) + ((RANGES-1)*sizeof(struct range)))

/*=============================
   low-level protocol help 
  =============================*/ 

// network order => local order 
extern void block_network_to_local(struct block *local, 
	struct block *net); 
extern void command_network_to_local(struct command *local, 
	struct command *network); 

// local order => network order 
extern void block_local_to_network(struct block *local, 
	struct block *net); 
extern void command_local_to_network(struct command *local, 
	struct command *network); 

/*=============================
   high-level protocol help 
  =============================*/ 

/* send a command to a server */ 
extern int send_command(int sockfd, const char *address, int port, 
			const char *filename, int first, int last);

/* send multiple commands to a server */
extern int send_commands(int sockfd, const char *address, int port, 
			const char *filename, struct range ranges[], int num);

/* receive a block from a server */ 
extern int recv_block(int sockfd, struct block *b, 
        struct sockaddr_in *resp_addr); 

/* run a "select" on input from servers, with timeout */ 
extern int select_block(int sockfd, int seconds, int microsec); 

/*=============================
   bit manipulation routines 
  =============================*/ 

#define BITSPERLONG (8*sizeof(long))
#define BITSIZE(BITS) \
	(((BITS)->nbits)/BITSPERLONG + ((((BITS)->nbits)%BITSPERLONG)!=0))

struct bits { 
    long *array; 
    int nbits; 
} ; 

extern void bits_alloc(struct bits *b, int nbits); 
extern void bits_free(struct bits *b); 
extern void bits_clearall(struct bits *b); 
extern void bits_setrange(struct bits *b, int first, int last); 
extern void bits_setbit(struct bits *b, int bit); 
extern void bits_clearbit(struct bits *b, int bit); 
extern int bits_testbit(struct bits *b, int bit); 
extern int bits_empty(struct bits *b); 
extern void bits_printall(struct bits *b); 
extern void bits_printlist(struct bits *b); 

/*=============================
  client-specific routines 
  =============================*/ 
extern void client_usage(); 
extern int client_arguments_valid(
	const char *server_dotted, int server_port, 
       int client_port, const char *filename); 

/*=============================
   halligan-specific routines
  =============================*/ 

/* read the primary IP address for an ECE/CS host */ 
extern int get_primary_addr(struct in_addr *a); 

#endif /* __HELP_H__ */ 
