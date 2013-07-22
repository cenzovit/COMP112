
#include <unistd.h>
#include <stdio.h> 
#include <netdb.h> 
#define FALSE 0
#define TRUE  1

#define MAX 1024
int getip(long *s) { 
     char buf[MAX]; 
     struct hostent *e; 
     if (gethostname(buf,MAX)) { 
	printf("cannot find host name\n"); 
	return FALSE; 
     }  
     printf("host name is %s\n",buf); 
     
     strcat(buf,".eecs.tufts.edu"); 
     e = gethostbyname(buf); 
     if (!e) { 
	printf("host name %s does not have an address record\n",buf);
	return FALSE; 
     } 
     if (e->h_addrtype!=PF_INET) { 
	printf("host name %s does not have correct address type\n",buf);
	return FALSE; 
     } 
     printf("%s number of address bytes is %d\n", buf,e->h_length); 
     *s = *(long *)(e->h_addr_list[0]); 
     printf("got ip address for %s\n",buf); 
     return TRUE; 
} 

int getip2(struct in_addr *ip) { 

} 

main() 
{ 
    long s; 
    char buf[MAX]; 
    if (getip(&s)) { 
     inet_ntop(PF_INET, (void *)(&s),  buf, MAX);
     printf("my ip address is %s\n",buf);
    } 

/* struct hostent {
      char    *h_name;        
      char    **h_aliases;   
      int     h_addrtype;   
      int     h_length;       
      char    **h_addr_list; 
 }
*/ 
     


} 
