/****************************************************
  file storage remembers size of file (= # of blocks)
  and block list. 
 ****************************************************/
#define BLOCKSIZE 512		/* size of one block */ 
#define MAXNAME 256     	/* size of a name */ 
#define MAXMESG (BLOCKSIZE*2)   /* longest message */ 
#define MAXADDR 80		/* max IP address in ascii format */ 

/* initialize by creating block arrays */ 
void storage_begin(int blocks) ; 
/* end use of block arrays */ 
void storage_end() ; 
/* store a file block at a specific physical block
   name: name of file
   fblock: file block number
   cblock: cache block number
   block: 512-byte buffer
 */
int remember_fblock_at_cblock(const char *name, 
	int fblock, int cblock, const char *block) ;
/* store a file block at next free physical block */ 
int remember_fblock(const char *name, int fblock, const char *block) ;
/* retrieve a file block into "block" */ 
int get_fblock(const char *name, int fblock, char *block) ; 
/* forget that we stored a file block */ 
int forget_fblock(const char *name, int fblock) ; 
/* determine whether we stored one */ 
int fblock_stored(const char *name, int fblock) ; 
/* forget a whole file: all records */ 
int forget_file(const char *name) ; 
/* determine whether a physical block "cblock" is in use */ 
int cblock_used(int cblock) ; 
/* forget whatever is in a physical block */ 
int forget_cblock(int cblock) ; 
/* return address of next cblock or -1 if none */ 
int next_cblock() ; 
/* memorize size of a file */ 
int remember_size(const char *name, int size) ;
/* recover the size */
int get_size(const char *name) ; 
/* forget a size */
int forget_size(const char *name) ; 
/* print memory */
void print_all() ; 
