// this is a UDP file service client based upon 
// a simple file completion strategy

#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <signal.h> 

#include "help.h" 
#define TRUE  1
#define FALSE 0 

/*================================
   global statistics 
  ================================*/ 
long bytes_sent=0; 
long packets_sent=0; 
long bytes_received=0; 
long packets_received=0; 
long bytes_lost=0; 
long packets_lost=0; 

void packet_received(int nbytes) { 
   bytes_received+=nbytes; packets_received++; 
} 
void packet_sent(int nbytes) { 
   bytes_sent+=nbytes; packets_sent++; 
} 
void packet_lost(int nbytes) { // phony output 
   bytes_lost+=nbytes; packets_lost++; 
} 
void packet_report() { 
    fprintf(stderr, "        %8ld packets received (%8ld bytes)\n", 
	packets_received, bytes_received); 
    fprintf(stderr, "server: %8ld packets sent     (%8ld bytes)\n", 
	packets_sent, bytes_sent); 
    fprintf(stderr, "        %8ld packets lost     (%8ld bytes)\n", 
	packets_lost, bytes_lost); 
} 

void handler(int sig) { 
    packet_report(); 
    exit(0); 
} 

/*================================
   bit field manipulation:
   quickly manipulate ranges of
   bits in a huge bitfield by using
   long arithmetic optimally. 
  ================================*/

uint64_t next_one(struct bits *bits, uint64_t which) { 
    uint64_t start = which; // random starting bit
    uint64_t i; 
    // check that there is a bit 
    if (bits_empty(bits)) return (-1); 
    // there must be a one somewhere or this loops infinitely
    for (i=start%bits->nbits; !bits_testbit(bits,i); i=(i+1)%bits->nbits) ; 
    // bit i is 1
    return i; 
} 

uint64_t random_one(struct bits *bits) { 
    return next_one(bits, lrand48()%bits->nbits); 
} 

/*================================
   job control: create a thread 
   that asynchronously responds 
   to clients (plural), managing
   multiple jobs and clients. 
  ================================*/
pthread_mutex_t worktodo; 
pthread_mutex_t safetomodify; 

struct job { 
    struct job *next; 			// linked list 
    struct bits todo; 			// bitfield of records to send
    int filefd; 			// open file descriptor 
    int sockfd; 			// bound socket 
    char filename[FILENAMESIZE]; 	// filename for reference 
    struct sockaddr_in client_addr; 	// recipient IP and port
    uint64_t which; 			// which block to do next 
}; 
struct job *head = NULL; /* all active jobs */ 
#define NORMAL 0
#define BUSY 1
#define GOOFY 2
int mode = NORMAL;       /* job processing mode */ 

/* this thread works on all active jobs 
   *simultaneously* and blocks itself when finished */ 
void *job_thread(void *input) { 
    while (1) { 
	fprintf(stderr, "server thread: waiting for work\n"); 
	pthread_mutex_lock(&worktodo); 	// block until there is work 
	fprintf(stderr, "server thread: working\n"); 
	while (head) { 			// at least one job
	    struct job *j=head; 
            while (j) { // for each job 
		// fprintf(stderr, "handling job %lx\n", j); 
		// if there is nothing to do, decommission the job 
		if (bits_empty(&j->todo)) { 
		    pthread_mutex_lock(&safetomodify); 
		    if (j==head) head=head->next; 
		    else { 
			// take the job out of the list 
			struct job *pred; 
			for (pred=head; 
                             pred && pred->next!=j; 
                             pred=pred->next) ; 
		        if (pred) pred->next = pred->next->next; 	
                    } 
		    // destroy the job descriptor
		    struct job *next=j->next; 
		    bits_free(&j->todo); 
		    close(j->filefd); // too many open files error if not
		    free(j); 
		    j=next; 
		    pthread_mutex_unlock(&safetomodify); 
		} else { 				// there is work to do
		    // probabilistically, with probability 10%, skip one list
		    if (mode==NORMAL
                     || ((mode==BUSY || mode==GOOFY) 
		      && (lrand48()%100>10))) { 
			if (mode!=GOOFY) { 
			    // do the next step from the job 
			    j->which = next_one(&j->todo, j->which);  
			} else { 
			    // do one random step from the job
			    j->which = random_one(&j->todo);  
			} 
			// fprintf(stderr, "server thread: trying to send block %d of %s\n", which, j->filename); 
			
			// 10% of the time, say you sent it and don't */ 
			if (mode==NORMAL
			 || ((mode==BUSY || mode==GOOFY) 
			  && (lrand48()%100>10))) { 
			    struct block loc_blk; 
			    memset(&loc_blk, 0, sizeof(loc_blk)); 
			    if (read_block(j->filefd, &loc_blk, j->which)!=0) { 
				fprintf(stderr, "server thread: error reading block %d of %s, aborting.\n", j->which, j->filename); 
				exit(1); 
			    } 
			    strncpy(loc_blk.filename, j->filename, FILENAMESIZE); 
			/* get readable internet address */
			    char client_dotted[INET_ADDRSTRLEN]; 	/* string ip address */ 
			    inet_ntop(PF_INET, (void *)&(j->client_addr.sin_addr.s_addr),  
				client_dotted, INET_ADDRSTRLEN);
			    int client_port        = ntohs(j->client_addr.sin_port);
			/* print action message */ 
			    fprintf(stderr, "server thread: sending block %d of file %s to %s:%d\n", 
				    j->which, j->filename, client_dotted, client_port); 

			/* send a message */ 
                            struct block_network net_blk; 
			    packet_sent(sizeof(net_blk)); 
			    block_local_to_network(&loc_blk,&net_blk); 
			    if (sendto(j->sockfd, &net_blk, sizeof(net_blk), 0, 
				(struct sockaddr *)&j->client_addr, sizeof(j->client_addr))<0) { 
				perror("sendto"); 
			    } 
			} else { 
			    fprintf(stderr, "server thread: NOT sending block %d of file %s (assume lost)\n", 
				    j->which, j->filename); 
			    packet_lost(sizeof(struct block)); 
			} 
		    /* mark that block as sent (whether you sent it or not) */ 
			pthread_mutex_lock(&safetomodify); 
			bits_clearbit(&j->todo, j->which); 
			pthread_mutex_unlock(&safetomodify); 
		    } 

		    // go do next list 
		    j=j->next; 
		} 
            } 
	} 
	fprintf(stderr, "server thread: idle\n"); 
	// pthread_mutex_lock(&worktodo); 	// block self: no more work now
    } 
    return NULL; 			// never reached 
} 

/*================================
  block formatting of transmitted files
  ================================*/ 
// read a specific block from a file
int read_block(int fd, struct block *local, uint64_t bnum) { 
    struct stat st; 
    fstat(fd, &st); 
    uint64_t bytes = st.st_size; 
    uint64_t blocks = bytes/PAYLOADSIZE; 
    if (bytes%PAYLOADSIZE!=0) blocks++; 
    local->which_block = bnum; 
    local->total_blocks = blocks; 
    uint64_t payload_start= bnum*PAYLOADSIZE;
    if (payload_start>=0 && payload_start<bytes) { 
	lseek(fd, payload_start, SEEK_SET); 
	local->paysize = read(fd, local->payload, PAYLOADSIZE); 
	return 0; 
     } else { 
	return -1; // non-existent block
     } 
} 

void server_usage() { 
    fprintf(stderr,"server usage: ./server {port} {seed} {mode}\n");
    fprintf(stderr,"  {port} is port to bind to on this machine\n"); 
    fprintf(stderr,"  {seed} is a random number seed from 1-99999\n"); 
    fprintf(stderr,"  {mode} is one of: \n"); 
    fprintf(stderr,"    normal -- send as fast as possible\n"); 
    fprintf(stderr,"    busy   -- inject some typical errors\n"); 
    fprintf(stderr,"    goofy  -- inject some ludicrous errors\n"); 
} 

/*================================
  main program: set up infrastructure
  and listen for UDP requests
  ================================*/ 
int main(int argc, char **argv)
{
    int sockfd;

/* my data */ 
    struct sockaddr_in my_addr; 	/* raw send address */ 

/* command data */ 
    struct sockaddr_in cmd_addr; 	/* raw recv address */ 
    int cmd_addrlen = sizeof(cmd_addr); /* length used */ 
    char cmd_dotted[INET_ADDRSTRLEN]; 	/* string ip address */ 
    int cmd_mesglen; 

/* receiver and sender share a port*/ 
    int port = 0; 
    int seed = 0; 

    signal(SIGINT, handler); // statistics reporting on control-C

    if (argc != 4) { 
	fprintf(stderr, "server: requires four arguments.\n"); 
	server_usage(); 
        exit(1); 
    } 

/* get port */ 
    port=atoi(argv[1]); 
    if (port<9000 || port>32767) { 
	fprintf(stderr, "server: port %d is not allowed\n", port); 
	server_usage(); 
	exit(1); 
    } 
/* get seed */ 
    seed=atoi(argv[2]); 
    if (seed<0 || seed>99999) { 
	fprintf(stderr, "server: seed %d must be between 0 and 99999\n", seed); 
	server_usage(); 
	exit(1); 
    } 
    srand48(seed); 
/* get mode */ 
    mode = NORMAL; 
    if (strncmp(argv[3], "normal", sizeof("normal")+1)==0) { 
	mode = NORMAL; 
    } else if (strncmp(argv[3], "busy", sizeof("busy")+1)==0) { 
	mode = BUSY; 
    } else if (strncmp(argv[3], "goofy", sizeof("goofy")+1)==0) { 
	mode = GOOFY; 
    } else { 
        fprintf(stderr, "server: unknown mode %s\n", argv[3]); 
	server_usage(); 
	exit(1); 
    } 
    
/* get the primary IP address of this host, 
   based upon Halligan hall network conventions */ 
    struct in_addr primary; 
    get_primary_addr(&primary); 
    char primary_dotted[INET_ADDRSTRLEN]; 
    inet_ntop(AF_INET, &primary, primary_dotted, INET_ADDRSTRLEN);
    fprintf(stderr, "server: Running on %s, port %d\n", 
	primary_dotted, port); 

/* make a socket*/ 
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);

/* construct an endpoint address with primary address and desired port */ 
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family      = PF_INET;
    memcpy(&my_addr.sin_addr,&primary,sizeof(struct in_addr)); 
    my_addr.sin_port        = htons(port);

/* bind it to an address and port on the sender */ 
    if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(my_addr))<0) { 
	perror("can't bind local address"); 
	exit(1); 
    } 
/* now a port is set up properly */ 

/* create a handler thread to respond to requests */ 
   int retval; /* unused; thread never returns */ 
   pthread_t thread1; 
   pthread_mutex_init(&worktodo, NULL);
   pthread_mutex_init(&safetomodify, NULL);
   pthread_mutex_lock(&worktodo); /* no work to do yet; save CPU */ 
   pthread_create(&thread1, NULL, job_thread, (void *)&retval); 

/* repeat forever */ 
    for (;;) { 
    /* receive a command */ 
	struct command_network net_cmd; 
        struct command loc_cmd; 
again: 
	cmd_addrlen = sizeof(cmd_addr); /* MUST initialize this */ 
	memset(&cmd_addr, 0, sizeof(cmd_addr));
	memset(&net_cmd, 0, sizeof(net_cmd)); 
	cmd_mesglen = recvfrom(sockfd, &net_cmd, sizeof(net_cmd), 0, 
	    (struct sockaddr *) &cmd_addr, &cmd_addrlen);
	packet_received(cmd_mesglen); 

    /* get numeric internet address */
	inet_ntop(PF_INET, (void *)&(cmd_addr.sin_addr.s_addr),  
	    cmd_dotted, INET_ADDRSTRLEN);
	int cmd_port = ntohs(cmd_addr.sin_port);
	// fprintf(stderr, "server: UDP request from %s:%d\n",
	//     cmd_dotted,cmd_port);

    /* convert to local order */ 
        command_network_to_local(&loc_cmd,&net_cmd); 

	if (cmd_mesglen != COMMAND_SIZE(loc_cmd.nranges)) { 
	    fprintf(stderr, 
		"server: command is wrong size for number of ranges, ignoring command\n"); 
	    goto again; 
	} 

    /* check filename for safety */ 
        if (index(loc_cmd.filename,'/')) { 
	    fprintf(stderr, "server: filename '%s' must not contain '/', ignoring command\n",
		loc_cmd.filename); 
	    goto again; 
        } 

      /* locate the job in the job list (if present) */ 
        pthread_mutex_lock(&safetomodify); 
	struct job *j; 
        for (j=head; 
             j && strncmp(j->filename,loc_cmd.filename,FILENAMESIZE)!=0; 
             j=j->next); 
        if (j) { 
        /* update existing job record for new request */ 
	   int range; 
	   // fprintf(stderr, "nranges = %d\n", loc_cmd.nranges); 
	   for (range=0; range<loc_cmd.nranges; range++) { 
	       // fprintf(stderr, " .. first=%d, last=%d\n", 
	       // 	loc_cmd.ranges[range].first_block, 
	       // 	loc_cmd.ranges[range].last_block) ; 
	       bits_setrange(&j->todo, 
			     loc_cmd.ranges[range].first_block, 
			     loc_cmd.ranges[range].last_block); 
	       fprintf(stderr, "server: %s:%d requests file %s, blocks %d to %d\n", 
		    cmd_dotted, cmd_port, j->filename, 
		    loc_cmd.ranges[range].first_block, 
		    loc_cmd.ranges[range].last_block); 
           } 
	   // bits_printall(&j->todo); 
	   pthread_mutex_unlock(&safetomodify); 
	   pthread_mutex_unlock(&worktodo); 
        } else { // new job
	   pthread_mutex_unlock(&safetomodify); 
	/* check that file exists */ 
	    int fd = open(loc_cmd.filename, O_RDONLY); 
	    if (fd<0) { 
		perror("open"); 
		fprintf(stderr, "server: file '%s' cannot be read, ignoring command\n",
		    loc_cmd.filename); 
		goto again; 
	    } 
	/* read initial block of file to set up parameters */ 
	    struct block loc_blk; 
	    if (read_block(fd, &loc_blk, 0L)==0) { 
		strncpy(loc_blk.filename, loc_cmd.filename, FILENAMESIZE); 
		memset(loc_blk.payload, 1, PAYLOADSIZE); // ANTIBUGGING flag
	    /* make up a job of blocks to be transferred */ 
		j = (struct job *) malloc (sizeof(struct job)); 
	    /* record the file to send */ 
		j->filefd = fd; 
	    /* and what socket to send it */ 
		j->sockfd = sockfd; 
	    /* init the "todo" array of blocks to be sent */ 
		bits_alloc(&j->todo,loc_blk.total_blocks); 
		bits_clearall(&j->todo); 
	    /* which block to start on */ 
		j->which = lrand48()%j->todo.nbits; 
	    /* save client destination */ 
		memcpy(&(j->client_addr), &cmd_addr, sizeof(struct sockaddr_in)); 
	    /* save file name */ 
		strncpy(j->filename,loc_cmd.filename,FILENAMESIZE); 
	    /* mark ranges */ 
		int range; 
		// fprintf(stderr, "nranges = %d\n", loc_cmd.nranges); 
		for (range=0; range<loc_cmd.nranges; range++) { 
		    // fprintf(stderr, " .. first=%d, last=%d\n", 
		    // 	loc_cmd.ranges[range].first_block, 
		    // 	loc_cmd.ranges[range].last_block) ; 
		   bits_setrange(&j->todo, 
			loc_cmd.ranges[range].first_block, 
			loc_cmd.ranges[range].last_block); 
		   fprintf(stderr, "server: %s:%d requests file %s, blocks %d to %d\n", 
			cmd_dotted, cmd_port, j->filename, 
			loc_cmd.ranges[range].first_block, 
			loc_cmd.ranges[range].last_block); 
		} 
	    /* add to job queue */ 
		pthread_mutex_lock(&safetomodify); 
		j->next=head; head=j; 
		pthread_mutex_unlock(&safetomodify); 
	    /* now job has been created, tell thread to wake up and do it */ 
		pthread_mutex_unlock(&worktodo); // safe to unlock even if unlocked
	    } else { 
		perror("read"); 
		fprintf(stderr, "server: block 0 of file '%s' cannot be read, ignoring command\n",
		    loc_cmd.filename); 
		goto again; 
	    } 
	} 
    } 
}
