// this is a UDP file service client based upon 
// a simple file completion strategy

/*=============================
  Starting solution for Assignment 2: UDP file transfer
  the intent of this file is to demonstrate how to 
  call the helper routines. It will help you avoid some
  common errors. IT DOES NOT SOLVE THE PROBLEM. 
  =============================*/ 

#include "help.h"
#include <stdio.h> 

#define TRUE  1
#define FALSE 0

int main(int argc, char **argv)
{
  
  /* local client data */ 
  int sockfd;		   		/* file descriptor for endpoint */ 
  struct sockaddr_in client_sockaddr;	/* address/port pair */ 
  struct in_addr client_addr; 	/* network-format address */ 
  char client_dotted[INET_ADDRSTRLEN];/* human-readable address */ 
  int client_port; 			/* local port */ 
  
  /* remote server data */ 
  char *server_dotted; 		/* human-readable address */ 
  int server_port; 			/* remote port */ 
  
  /* the request */ 
  char *filename; 			/* filename to request */ 
  
  /* read arguments */ 
  if (argc != 5) { 
    fprintf(stderr, "client: wrong number of arguments\n"); 
    client_usage(); 
    exit(1); 
  } 
  
  server_dotted = argv[1]; 
  server_port = atoi(argv[2]); 
  client_port = atoi(argv[3]); 
  filename = argv[4]; 
  
  if (!client_arguments_valid(
			      server_dotted, server_port, 
			      client_port, filename)) { 
    client_usage(); 
    exit(1); 
  } 
  
  /* get the primary IP address of this host */ 
  get_primary_addr(&client_addr); 
  inet_ntop(AF_INET, &client_addr, client_dotted, INET_ADDRSTRLEN);
  
  /* construct an endpoint address with primary address and desired port */ 
  memset(&client_sockaddr, 0, sizeof(client_sockaddr));
  client_sockaddr.sin_family      = PF_INET;
  memcpy(&client_sockaddr.sin_addr,&client_addr,sizeof(struct in_addr)); 
  client_sockaddr.sin_port        = htons(client_port);
  
  /* make a socket*/ 
  sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd<0) { 
    perror("can't open socket"); 
    exit(1); 
  } 
  
  /* bind it to an appropriate local address and port */ 
  if (bind(sockfd, (struct sockaddr *) &client_sockaddr, 
	   sizeof(client_sockaddr))<0) { 
    perror("can't bind local address"); 
    exit(1); 
  } 
  fprintf(stderr, "client: Receiving on %s, port %d\n", 
	  client_dotted, client_port); 
  
  /* send a command */ 
  send_command(sockfd, server_dotted, server_port, filename, 0, MAXINT); 
  
  fprintf(stderr, "client: requesting %s blocks %d-%d\n", 
	  filename, 0, MAXINT); 
  
  /* receive the whole document and make naive assumptions */ 
  int done = FALSE; // set to TRUE when you think you're done
  int init = TRUE;
  
  /* create a bit array to test which blocks have been received */
  struct bits blocksRecv;
  
  /* open a file to write to in the current directory */
  FILE *download = fopen(filename, "w");
  fclose(download);
  int outfd = open(filename, O_RDWR);
  
  while (!done) { 
    int retval;
  again: 
    if ((retval = select_block(sockfd, 0, 20000))==0) { 
      /* timeout */
      struct range nBlocks[12];
      int currentRange = 0;
      int i;
      int check = 0;
      int numBlocks = blocksRecv.nbits;
      for(i = 0; i < numBlocks; i++){
	/* look for the start of a missed block */
	if(check == 0 && bits_testbit(&blocksRecv, i)){
	  nBlocks[currentRange].first_block = i;
	  nBlocks[currentRange].last_block = numBlocks-1;
	  check++;
	}
	/* look for the end of a missed block */
	else if(check == 1 && !(bits_testbit(&blocksRecv, i))){
	  nBlocks[currentRange].last_block = i-1;
	  currentRange ++;
	  /* if you have found 12 missed blocks, send_commands */
	  if(currentRange == 12){
	    send_commands(sockfd, server_dotted, server_port, filename, nBlocks, currentRange);
	    currentRange = 0;
	  }

	  check = 0;
	}
      }
      /* send any left over blocks from the loop above */
      send_commands(sockfd, server_dotted, server_port, filename, nBlocks, currentRange);
      
      if(bits_empty(&blocksRecv)){
	done = TRUE;
      }
      
    } else if (retval<0) { 
      /* error */ 
      perror("select"); 
      fprintf(stderr, "client: receive error\n"); 
    } else { 
      /* input is waiting, read it */ 
      struct sockaddr_in resp_sockaddr; 	/* address/port pair */ 
      int resp_len; 			/* length used */ 
      char resp_dotted[INET_ADDRSTRLEN]; 	/* human-readable address */ 
      int resp_port; 			/* port */ 
      int resp_mesglen; 			/* length of message */ 
      struct block one_block; 
      
      /* use helper routine to receive a block */ 
      recv_block(sockfd, &one_block, &resp_sockaddr);
      
      /* get human-readable internet address */
      inet_ntop(AF_INET, (void *)&(resp_sockaddr.sin_addr.s_addr),  
		resp_dotted, INET_ADDRSTRLEN);
      resp_port = ntohs(resp_sockaddr.sin_port); 
      
      fprintf(stderr, "client: %s:%d sent %s block %d (range 0-%d)\n",
	      resp_dotted, resp_port, one_block.filename, 
	      one_block.which_block, one_block.total_blocks);
      
      /* check block data for errors */
      if (strcmp(filename, one_block.filename)!=0) { 
	fprintf(stderr, 
		"client: received block with incorrect filename %s\n", 
		one_block.filename); 
	goto again; 
      } 

      /* init the bit array once you know how many blocks the file contains */
      if(init){
	//fprintf(stderr, "total blocks: %d\n", one_block.total_blocks);
	bits_alloc(&blocksRecv, one_block.total_blocks);
	bits_setrange(&blocksRecv, 0, one_block.total_blocks - 1); 
	init = FALSE;
      }

      /* if you have not received the current block, write it and flag it as received */
      if(bits_testbit(&blocksRecv, one_block.which_block)){
	bits_clearbit(&blocksRecv, one_block.which_block);
	lseek(outfd, one_block.which_block*PAYLOADSIZE, SEEK_SET);
	write(outfd, one_block.payload, one_block.paysize);
      }
            
      /* if all blocks have been received, done */
      if(bits_empty(&blocksRecv)){
	done = TRUE;
      }
    } 
  }
  /* close the file stream */
  close(outfd);
}


  
