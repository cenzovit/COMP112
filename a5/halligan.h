#include <stdio.h>      
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>

// read the primary IP address for an ECE/CS host 
// this is used to avoid listening (by default) on 
// maintenance interfaces. 
int get_primary_addr(struct in_addr *a); 
