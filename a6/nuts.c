/* simple tcp client based upon Stevens examples 
   Source: Stevens, "Unix Network Programming" */ 

#include <string.h>
#include <unistd.h> 
#include <stdlib.h> 
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdarg.h> 

#define SERV_TCP_PORT 8000 /* server's port number */
#define MAXNAME 256		/* maximum name length */ 
#define MAXMESG (MAXNAME*2)	/* maximum command length */ 
#define MAXADDR 80		/* max size of ip address */ 
#define MAXOUT 256 		/* maximum number of output chars for flog */ 
#define BLOCKSIZE 256

#define TRUE 1
#define FALSE 0

/* logging of server actions */ 
static void flog(const char *fmt, ...) {
    va_list ap;
    char *p; 
    char buf[MAXOUT]; 
    va_start(ap, fmt);
    fprintf(stderr,"[nuts: "); 
    vsnprintf(buf,MAXOUT,fmt,ap); 
    for (p=buf; *p && p-buf<MAXOUT; p++) 
	if ((*p)>=' ') 
	    putc(*p,stderr); 
    fprintf(stderr,"]\n"); 
    va_end(ap);
} 

#define GET 0
#define PUT 1
#define DEL 2
static const char *actions[3]= {"get","put","del"}; 

// void print_binary(const char *buf, int len) { 
//     int i; 
//     for (i=0; i<len; i++) { 
// 	if (i%8==0) fprintf(stderr,"\n%8x: ",i); 
// 	fprintf(stderr,"%2x ",buf[i]); 
//     } 
//     fprintf(stderr,"\n"); 
// } 

/* read a line of input from a socket */ 
int read_line(int sock, char *stuff, int len) { 
    int size; 
    int count = 0; 
    while (count<len && (size=read(sock,stuff,1))>0 && *stuff != '\n') {
	stuff++; count++; 
    } 
    if (count<len && size>0) { stuff++; count++; } /* skip \n */ 
    if (count<len) { *stuff='\0'; } 
    return count; 
} 

int read_block(int sock, char *stuff, int size) { 
    int c; 
    int count=0; 
    do { 
	c=read(sock,stuff,size); 
	if (c>0) { size-=c; stuff+=c; count+=c; } 
	else return count; 
    } while (size>0); 
    return count; 
} 


/* number of bytes in file */ 
unsigned int bytes(int fd) { 
    struct stat buf; 
    if (!fstat(fd,&buf)) { 
	return buf.st_size; 
    } 
    else return 0xffffffff; 
} 

/* this takes a full pathname and ensures that it exists */ 
int checkpathexists(const char *path)  {
    struct stat buf; 
    if (!stat(path,&buf)) return 1; 
    else return 0; 
} 

/* this takes a full pathname and makes sure it's a regular file */ 
int checkregfile(const char *path) { 
    struct stat buf; 
    if (!stat(path,&buf)) { 
	return S_ISREG(buf.st_mode); 
    } 
    else return 0;
} 

void usage() { 
    fprintf(stderr,"nuts usage: nuts <port> <action> <filename>\n"); 
    fprintf(stderr,"where:\n"); 
    fprintf(stderr,"- <port> is the port number to use.\n"); 
    fprintf(stderr,"- <action> is get, put, or del.\n"); 
    fprintf(stderr,"- <filename> is a filename to operate upon.\n"); 
}

int main(int argc, char *argv[])
{
    int errors=0; 		 	/* number of errors */ 

    /* networking variables */ 
    int sockfd;				/* socket number */ 
    struct sockaddr_in serv_addr;	/* address of remote socket */ 
    char *serv_host = "localhost";      /* default hostname */ 
    struct hostent *serv_hostent;	/* host entry for DNS lookup */ 

    /* message variables */ 
    char request[MAXMESG]; 
    char response[MAXMESG]; 	  	/* response from server */ 
    int resplen; 			/* length of response */ 

    /* parameters */ 
    int action=GET; 			/* default command */ 
    char filename[MAXNAME]; 		/* max filename */ 
    int filesize=0; 		        /* file size in bytes */ 
    int serv_port = SERV_TCP_PORT;      /* default server port */ 

    /* command line: client port action filename */
    if (argc!=4) { 
	fprintf(stderr,"nuts requires three arguments, got %d\n",argc-1); 
	usage(); 	
	exit(1); 
    } 

    if (sscanf(argv[1],"%d",&serv_port)!=1) { 
	fprintf(stderr,"port number is not a number\n"); 
	errors++; 
    } else if (serv_port<8000 || serv_port>65000)  {
	fprintf(stderr,"port number %d is invalid\n",serv_port); 
	errors++; 
    } 
    if (strcmp(argv[2],"get")==0) { 
	action=GET; 
    } else if (strcmp(argv[2],"put")==0) { 
	action=PUT; 
    } else if (strcmp(argv[2],"del")==0) { 
	action=DEL; 
    } else { 
	fprintf(stderr,"unknown action %s\n",argv[2]); 
	errors++; 
    } 
    if (strlen(argv[3])>255) { 
	fprintf(stderr,"file name too long (limited to 256 characters)\n");
	errors++; 
    } else { 
	strcpy(filename,argv[3]); 
    } 
    if (errors) { 
	usage(); 
	exit(1); 
    } 
    flog("port=%d action=%s server=%s filename=%s\n",
	serv_port, actions[action], serv_host, filename); 

    /* get the IP address of the host */
    if((serv_hostent = gethostbyname(serv_host)) == NULL) {
	perror("gethostbyname error");
	exit(1);
    }

    if(serv_hostent->h_addrtype !=  PF_INET) {
	fprintf(stderr,"unknown address type\n"); 
	exit(1);
    }

    /* construct an address for the socket 
       consisting of the address and serv_port of the host */ 
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = PF_INET;

    /* address from gethostbyname */ 
    serv_addr.sin_addr.s_addr = 
       ((struct in_addr *)serv_hostent->h_addr_list[0])->s_addr;
	   /* host record ^^^^^^^^^^^^ */
		    /* all ip addresses ^^^^^^^^^^^ */ 
						/* ^^^ first one */ 
    /* port from command line or data */ 
    serv_addr.sin_port = htons(serv_port);
    
    if (action==GET) { 
	/* write a message to the server */
	sprintf(request, "get %s\n",filename); 
	/* open a TCP socket */
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("can't open stream socket");
	    exit(1);
	}

	/* connect to the server */    
	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
	    perror("can't connect to server");
	    exit(1);
	}

	flog("client sends: %s\n",request); 
	write(sockfd, request, strlen(request));
	resplen=read_line(sockfd, response, MAXMESG); 
	response[resplen]='\0'; 
	flog("client receives: %s\n",response); 
	if (strncmp(response,"100 ok ",7)==0) {
	    char *buf; 
	    int size; 
	    flog("request succeeded"); 
            size=atoi(response+7); 
	    buf = (char *)malloc(size*sizeof(char)); 
	    flog("receiving %d bytes",size); 
	    int newsize =read_block(sockfd, buf, size);
	    if (size != newsize) { 
		flog("requested %d bytes, got %d",size,newsize); 
	    } 
	    flog("writing file to stdout"); 
            fwrite(buf,1,newsize,stdout); 
	    free(buf); 
	} else { 
	    flog("request failed"); 
	} 
    } else if (action==PUT) { 
	if (!checkpathexists(filename)) { 	/* path must exist */ 
	    fprintf(stderr, "path %s does not exist\n",filename); 
	    exit(1); 
	} 
	if (!checkregfile(filename)) { 		/* path must be file */ 
	    fprintf(stderr, "path %s is not a regular file\n",filename); 
	    exit(1); 
	
        } 
	FILE *f = fopen(filename, "r"); 
        filesize = bytes(fileno(f)); 

	/* ready to write; open a TCP socket */
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("can't open stream socket");
	    exit(1);
	}

	/* connect to the server */    
	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
	    perror("can't connect to server");
	    exit(1);
	}
	/* write a message to the server */
	sprintf(request, "put %s %d\n",filename,filesize); 
	flog("client sends: %s\n",request); 
	write(sockfd, request, strlen(request));
	int back = 1; 
        char buf[1024]; 
        while (!feof(f) && (back=fread(buf,1,1024,f))) { 
	      write(sockfd,buf,back); 
	} 
	fclose(f); 
	resplen=read_line(sockfd, response, MAXMESG); 
	response[resplen]='\0'; 
	flog("client receives: %s\n",response); 
    } else if (action==DEL) { 
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	    perror("can't open stream socket");
	    exit(1);
	}

	/* connect to the server */    
	if(connect(sockfd, (struct sockaddr *) &serv_addr, 
		   sizeof(serv_addr)) < 0) {
	    perror("can't connect to server");
	    exit(1);
	}
	sprintf(request, "del %s\n",filename); 
	flog("client sends: %s\n",request); 
	write(sockfd, request, strlen(request));
	resplen=read_line(sockfd, response, MAXMESG); 
	response[resplen]='\0'; 
	flog("client receives: %s\n",response); 
    } 
    close(sockfd);
}
