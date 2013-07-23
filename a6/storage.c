/****************************************************
  file storage remembers size of file (= # of blocks)
  and block list. 
 ****************************************************/
#include <stdio.h> 
#include <stdarg.h>
#include <unistd.h> 
#include <stdlib.h> 
#include <string.h> 
#include "storage.h" 

#define MAXOUT 256 	/* maximum number of output chars for flog */ 
#define TRUE 1
#define FALSE 0
#define ASSERT(X) if (!(X)) flog("ASSERTION '%s' FAILED in line %d of file %s",#X,__LINE__,__FILE__)

static char *storage = NULL; 
static int *storage_used = NULL;
static int storage_blocks = 0; /* set from argv */ 

static void storage_remember(int); 
static void storage_forget(int); 

/* logging of server actions */ 
static void flog(const char *fmt, ...) {
    va_list ap;
    char *p; 
    char buf[MAXOUT]; 
    va_start(ap, fmt);
    fprintf(stderr,"[storage: "); 
    vsnprintf(buf,MAXOUT,fmt,ap); 
    for (p=buf; *p && p-buf<MAXOUT; p++) 
	if ((*p)>=' ') 
	    putc(*p,stderr); 
    fprintf(stderr,"]\n"); 
    va_end(ap);
} 

/****************************************************
   block descriptors store idea of fblock 
   file block corresponds to what cache slot 
 ****************************************************/ 

struct map { 
    struct map *next; 
    int fblock; /* file block */ 
    int cblock; /* cache slot */ 
} ; 
typedef struct map Map; 

static Map *map_new(int fblock, int cblock) { 
    Map *b = (Map *) malloc (sizeof(Map)); 
    b->fblock=fblock; b->cblock=cblock; 
    return b; 
} 

static void map_print(Map *head) { 
    Map *b; 
    fprintf(stderr,"  map:\n"); 
    for (b=head; b; b=b->next) { 
	fprintf(stderr,"    block: %d->%d\n",b->fblock,b->cblock);
	ASSERT(b->cblock>=0 && b->cblock<storage_blocks); 
	ASSERT(storage_used[b->cblock]!=0); 
    } 
} 

/* add a block descriptor to a descriptor list */ 
static Map *map_add(Map **head, int fblock, int cblock) { 
    Map *b; 
    if (!*head || fblock<(*head)->fblock) { 
	Map *c = map_new(fblock,cblock); 
	c->next=*head; *head=c; 
	ASSERT(cblock>=0 && cblock<storage_blocks); 
	ASSERT(storage_used[cblock]==0); 
	storage_remember(cblock); 
	return c; 
    } 
    for (b=*head; b->next && b->fblock!=fblock; b=b->next) ; 
    if (b->fblock==fblock) { 
	if (cblock!=b->cblock) { 
	    flog("using new cblock location %d (old %d) for block %d",
	 	cblock,b->cblock,fblock);
	    ASSERT(b->cblock>=0 && b->cblock<storage_blocks); 
	    ASSERT(storage_used[b->cblock]!=0); 
	    storage_forget(b->cblock); 
	    b->cblock=cblock; 
	    storage_remember(b->cblock); 
	} 
	ASSERT(storage_used[b->cblock]!=0); 
	return b; 
    } else { 
	Map *c = map_new(fblock,cblock); 
	c->next=NULL; b->next=c; 
	ASSERT(cblock>=0 && cblock<storage_blocks); 
	ASSERT(storage_used[cblock]==0); 
	storage_remember(cblock); 
        return c;
    } 
} 
    
/* remove a map to a file block */ 
static void map_forget_fblock(Map **head, int fblock) { 
    Map *b; 
    if (!*head) return; 
    if ((*head)->fblock==fblock) { 
	Map *e=*head; *head=(*head)->next; 
	ASSERT(e->cblock>=0 && e->cblock<storage_blocks); 
	ASSERT(storage_used[e->cblock]!=0); 
	storage_forget(e->cblock); 
	free(e);
    } 
    for (b=*head; b && b->next && b->next->fblock != fblock; b=b->next) ; 
    if (b && b->next && b->next->fblock==fblock) { 
	Map *e = b->next; b->next = b->next->next; 
	ASSERT(e->cblock>=0 && e->cblock<storage_blocks); 
	ASSERT(storage_used[e->cblock]!=0); 
	storage_forget(e->cblock); 
	free(e); 
    } 
} 
    
/* remove a map to a physical block */ 
static void map_forget_cblock(Map **head, int cblock) { 
    Map *b; 
    if (!*head) return; 
    if ((*head)->cblock==cblock) { 
	Map *e=*head; *head=(*head)->next; 
	ASSERT(e->cblock>=0 && e->cblock<storage_blocks); 
	ASSERT(storage_used[e->cblock]!=0); 
	storage_forget(e->cblock); 
	free(e); return; 
    } 
    for (b=*head; b && b->next && b->next->cblock!=cblock; b=b->next) ; 
    if (b && b->next && b->next->cblock==cblock) { 
	Map *e = b->next; b->next = b->next->next; 
	ASSERT(e->cblock>=0 && e->cblock<storage_blocks); 
	ASSERT(storage_used[e->cblock]!=0); 
	storage_forget(e->cblock); 
	free(e); return; 
    } 
} 

/* return the map descriptor containing a physical block, if any */
static Map *map_cblock_used(Map *head, int cblock) { 
    Map *b; 
    for (b=head; b && b->cblock!=cblock; b=b->next) 
	ASSERT(b->cblock>=0&&b->cblock<storage_blocks&&storage_used[b->cblock]); 
    if (b && b->cblock==cblock) return b; 
    return NULL; 
} 

/* determine whether a file block is stored in a map */ 
static Map *map_fblock_stored(Map *head, int fblock) { 
    Map *b; 
    for (b=head; b && b->fblock!=fblock; b=b->next) {
	ASSERT(b->cblock>=0 && b->cblock<storage_blocks); 
	ASSERT(storage_used[b->cblock]!=0); 
    } 
    if (b && b->fblock==fblock) return b;
    return NULL; 
} 
    
/* clear out linked list of blocks */ 
static void map_clear(Map **head) { 
    Map *first,*next; 
    for (first=*head; first; first=next) { 
	next=first->next; 
	ASSERT(first->cblock>=0 && first->cblock<storage_blocks); 
	ASSERT(storage_used[first->cblock]!=0); 
	storage_forget(first->cblock); 
	free(first); 
    } 
    *head=NULL; 
} 

/****************************************************
   size descriptors store total sizes of files 
 ****************************************************/ 

struct size { 
    struct size *next; 
    char name[MAXNAME]; 
    int size; 
} ; 
typedef struct size Size;

Size *size_head = NULL; 

static Size *size_new(const char *name,int size) { 
    Size *s = (Size *)malloc (sizeof(Size)); 
    strcpy(s->name,name); 
    s->size=size;
    return s; 
} 

static Size *size_remember(const char *name, int size) { 
    if (!size_head || strcmp(name,size_head->name)<0) { /* before head */ 
	Size *e = size_new(name,size); 
	e->next=size_head; size_head=e; 
	return e; 
    } else { 
	Size *f; 
	for (f=size_head; f->next && strcmp(name,f->next->name)>0; f=f->next) ;
	if (strcmp(f->name,name)==0) { /* exact match */ 
	    f->size=size; 
	    return f; 
	} else { // list name < name => insert
	    Size *e = size_new(name,size); 
	    e->next = f->next; f->next=e; 
	    return e; 
	} 
    } 
} 

static int size_get(const char *name) { 
    Size *f; 
    for (f=size_head; f && strcmp(name,f->name)!=0; f=f->next) ;
    if (f && strcmp(f->name,name)==0) { /* exact match */ 
	return f->size; 
    } else { // list name < name => insert
	return -1;
    } 
} 

/* remove one thing from size list */ 
static void size_forget(const char *name) { 
    Size *f; 
    if (!size_head) return; 
    else if (strcmp(size_head->name,name)==0) { 
	Size *e=size_head; 
	size_head=size_head->next; free(e);  
    } else { 
	for (f=size_head; f && f->next; f=f->next) {
	    if (strcmp(f->next->name,name)==0) { 
		Size *e = f->next; 
		f->next = f->next->next; free(e); 
	    } 
	} 
    } 
    return; 
} 

static void size_print() { 
    Size *f; 
    fprintf(stderr,"size list: \n"); 
    for (f=size_head; f; f=f->next) { 
	fprintf(stderr, "  %s -> %d\n",f->name, f->size); 
    } 
} 

/****************************************************
   content descriptors store correspondence between 
   file blocks and cache blocks. 
 ****************************************************/ 

struct content { 
    struct content *next; 
    char name[MAXNAME]; 
    Map *bhead; 
} ; 
typedef struct content Content; 

static Content *content_new(const char *name) { 
    Content *e = (struct content *) malloc (sizeof(struct content)); 
    strcpy(e->name,name); 
    e->next=NULL; e->bhead=NULL; 
    return e; 
} 

static Map *content_cblock_used(Content *f, int cblock) { 
    return map_cblock_used(f->bhead,cblock); 
} 
static void content_forget_cblock(Content *f, int cblock) { 
    map_forget_cblock(&f->bhead,cblock); 
} 
static Map *content_fblock_stored(Content *f, int fblock) { 
    return map_fblock_stored(f->bhead,fblock); 
} 
static void content_forget_fblock(Content *f, int fblock) { 
    map_forget_fblock(&f->bhead,fblock); 
} 

/************************************************************
  a list of file descriptors describes all of cblock storage
 ************************************************************/
static Content *content_head = NULL; 

/* maintain a sorted list of file names; add if not present */ 
static Content *content_add(const char *name) { 
    if (!content_head) { /* list empty */ 
	Content *e = content_new(name); 
	e->next=NULL; content_head=e; 
	flog("link %s at head",name); 
	return e; 
    } else { 
	Content *f; 
	for (f=content_head; f->next && strcmp(name,f->name)!=0; f=f->next) ;
	if (strcmp(f->name,name)==0) { /* exact match */ 
	    flog("reuse record for %s",name); 
	    return f; 
	} else { // at tail => insert
	    Content *e = content_new(name); 
	    e->next = NULL; f->next=e; 
	    flog("link %s after %s",name,f->name); 
	    return e; 
	} 
    } 
} 

/* grab a record without add */ 
static Content *content_get(const char *name) { 
    Content *f; 
    for (f=content_head; f && strcmp(name,f->name)!=0; f=f->next) ;
    if (f && strcmp(f->name,name)==0) { /* exact match */ 
	return f; 
    } else { // list name < name => insert
	return NULL; 
    } 
} 

/* delete one name descriptor */ 
static void clist_forget(Content *target) { 
    Content *f; 
    if (!content_head) return; 
    if (target==content_head) { 
	content_head=content_head->next; 
        map_clear(&target->bhead); 
        free(target); return; 
    } 
    for (f=content_head; f && f->next!=target; f=f->next) ; 
    if (f) { 
	f->next=f->next->next; 
	map_clear(&target->bhead); 
	free(target); 
    } 
    return; 
} 

static void clist_print() { 
    Content *f; 
    fprintf(stderr,"content list:\n"); 
    for (f=content_head; f; f=f->next) { 
	fprintf(stderr,"  name: %s\n",f->name); 
	map_print(f->bhead); 
    }
} 

static Content *clist_cblock_used(int cblock) { 
    Content *h; 
    for (h=content_head; h; h=h->next) { 
	if (content_cblock_used(h,cblock)) return h; 
    } 
    return NULL; 
} 
#if 0
static void clist_forget_fblock(int fblock) { 
    Content *h; 
    for (h=content_head; h; h=h->next) { 
	if (content_fblock_stored(h,fblock)) 
	    content_forget_fblock(h,fblock);
    } 
} 
static Content *clist_fblock_stored(int fblock) { 
    Content *h; 
    for (h=content_head; h; h=h->next) { 
	if (content_fblock_stored(h,fblock)) return h; 
    } 
    return NULL; 
} 
static void clist_forget_cblock(int cblock) { 
    Content *h; 
    for (h=content_head; h; h=h->next) { 
	if (content_cblock_used(h,cblock)) 
	    content_forget_cblock(h,cblock);
    } 
} 
#endif 

/**********************************************************
   the "storage" routines emulate a fixed-size buffer file
   one can quite easily replace these with file I/O 
   for a more practical solution. 
 **********************************************************/

/* put one block into storage */ 
static void storage_put(int cblock, const char *what) { 
    flog("storage_put(%d,...)",cblock);
    int offset = cblock*BLOCKSIZE; int i; 
    for (i=0; i<BLOCKSIZE; i++) { 
	storage[offset++]=what[i]; 
    } 
    /* storage_used[cblock]=TRUE; */ 
} 

/* get one block out of storage */ 
static void storage_get(int cblock, char *what) { 
    flog("storage_get(%d,...)",cblock);
    if (!storage_used[cblock]) { 
	flog("attempt to get unused cblock %d\n",cblock); 
    } 
    int offset = cblock*BLOCKSIZE; int i; 
    for (i=0; i<BLOCKSIZE; i++) { 
	what[i]=storage[offset++]; 
    } 
} 

#if 0 
/* clear one block */ 
static void storage_clear(int cblock) { 
    int offset = cblock*BLOCKSIZE; int i; 
    flog("storage_clear(%d)",cblock);
    ASSERT(cblock>=0 && cblock<storage_blocks); 
    for (i=0; i<BLOCKSIZE; i++) { 
	storage[offset++]=0; 
    } 
} 
#endif 

/* mark one block used */ 
static void storage_remember(int cblock) { 
    flog("storage_remember(%d)",cblock);
    ASSERT(cblock>=0 && cblock<storage_blocks); 
    ASSERT(storage_used[cblock]==0); 
    storage_used[cblock]=TRUE; 
} 

/* free one block for reuse */ 
static void storage_forget(int cblock) { 
    flog("storage_forget(%d)",cblock);
    ASSERT(cblock>=0 && cblock<storage_blocks); 
    ASSERT(storage_used[cblock]!=0); 
    storage_used[cblock]=FALSE; 
} 

/* determine next entry to use */ 
static int storage_next() { 
    int i; 
    for (i=0; i<storage_blocks; i++) 
	if (!storage_used[i]) return i; 
    return -1; 
} 

/* print storage status */ 
static void storage_print() { 
    int i; int count=0; 
    int first=-1;
    fprintf(stderr, "allocated blocks:\n"); 
    for (i=0; i<storage_blocks; i++) {
	if (storage_used[i]) {
	    if (first<0) first=i; 
	} else { 
	    if (first>=0) {
		fprintf(stderr,"  %d..%d",first,i-1); first=-1;
		count++; if (count!=0 && count%8==0) fprintf(stderr,"\n"); 
	    } 
        } 
    } 
    if (first>=0) {
	fprintf(stderr,"  %d..%d",first,i-1); first=-1;
	count++; if (count!=0 && count%8==0) fprintf(stderr,"\n"); 
    } 
    if (count%8!=0) fprintf(stderr,"\n"); 
} 

/****************************************************************
    high-level routines allow storing and retrieving one block
   of a file, and enforce assignment restrictions 
 ****************************************************************/

/* initialize storage */ 
void storage_begin(int blocks) { 
    int i; 
    flog("storage_begin(%d)",blocks);
    storage_blocks = blocks; 
    storage = (char *) malloc(storage_blocks*BLOCKSIZE*sizeof(char)); 
    for (i=0; i<storage_blocks*BLOCKSIZE; i++) storage[i]=0; 
    storage_used = (int *)malloc(storage_blocks*sizeof(int)); 
    for (i=0; i<storage_blocks; i++) storage_used[i]=FALSE; 
} 

/* free all storage at end of run */ 
void storage_end() { 
    flog("storage_end()");
    free(storage); 
    free(storage_used); 
} 

/* map a file block to a specific cache block */
int remember_fblock_at_cblock(const char *name, int fblock, int cblock, const char *block) {
    Content *f, *g; 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    if (cblock<0 || cblock>=storage_blocks) {
	flog("cache block %d does not exist",cblock); 
	return FALSE; 
    } 
    f = content_add(name); 
    if ((g=clist_cblock_used(cblock))) { 
	Map *b=content_cblock_used(g,cblock); 
	flog("cache block %d in use for (%s %d) (removing)",cblock,g->name,b->fblock); 
	content_forget_cblock(g,cblock); 
    } 
    Map *b; 
    if ((b=content_fblock_stored(f,fblock))) { 
	flog("file block %d already stored for file %s (local %d) (removing)",fblock,f->name,b->cblock); 
	storage_forget(b->cblock); 
	content_forget_fblock(f,fblock); 
    } 
    storage_put(cblock, (const char *)block); 
    map_add(&(f->bhead),fblock,cblock); 
    flog("stored block %d of %s (cblock %d)",fblock,name,cblock); 
    return TRUE; 
} 

/* remember a file block using next available cache block */ 
int remember_fblock(const char *name, int fblock, const char *block) {
    Content *f; 
    Map *b; 
    int cblock; 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    if (!block) { 
	flog("can't read from NULL pointer"); 
	return FALSE; 
    } 
    f = content_add(name); 
    if ((b=content_fblock_stored(f,fblock))) { 
	cblock=b->cblock; 
	flog("reused old cache block %d",cblock); 
    } else {
	cblock=next_cblock(); 
	flog("chose new cache block %d",cblock); 
    } 
    storage_put(cblock, (const char *)block); 
    map_add(&(f->bhead),fblock,cblock); 
    flog("stored block %d of %s (cblock %d)",fblock,name,cblock); 
    return TRUE; 
} 

/* read file block from cache */ 
int get_fblock(const char *name, int fblock, char *block) { 
    Content *f = content_get(name); 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    if (f<0) { 
	flog("no blocks for name %s",name); 
	return FALSE; 
    } 
    if (!block) { 
	flog("can't write to NULL pointer"); 
	return FALSE; 
    } 
    Map *b = content_fblock_stored(f,fblock); 
    if (b==NULL) { 
	flog("block %d of %s not stored",fblock,name); 
	return FALSE; 
    } 
    storage_get(b->cblock,block); 
    flog("retrieved block %d of %s (cblock %d)",fblock,name,b->cblock); 
    return TRUE; 
} 

/* forget one file block */ 
int forget_fblock(const char *name, int fblock) { 
    Content *f; 
    Map *b; 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    f = content_get(name); 
    if (f==NULL) { 
	flog("no blocks for name %s",name); 
	return FALSE; 
    } 
    if ((b=map_fblock_stored(f->bhead,fblock))==NULL) { 
	flog("block %d of name %s not stored",name,fblock); 
	return FALSE;
    } 
    map_forget_fblock(&f->bhead,fblock); 
    return TRUE; 
} 

/* determine whether a file block is mapped */ 
int fblock_stored(const char *name, int fblock) { 
    Content *f; 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    f = content_get(name); 
    if (!f) return FALSE; 
    Map *b = map_fblock_stored(f->bhead,fblock); 
    return b!=0;
} 

/* forget all records for a file */ 
int forget_file(const char *name) { 
    Content *f; 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    f = content_get(name); 
    if (f==NULL) { 
	flog("no file '%s' to forget",name); 
	return FALSE; 
    } 
    clist_forget(f);
    return TRUE; 
} 

int cblock_used(int cblock) { 
    if (cblock<0 || cblock>=storage_blocks) { 
	flog("cache block %d out of range",cblock);
	return FALSE; 
    } 
    return storage_used[cblock]; 
} 

int forget_cblock(int cblock) { 
    Content *f; 
    if (cblock<0 || cblock>=storage_blocks) { 
	flog("cache block %d out of range",cblock);
	return FALSE; 
    } 
    f = clist_cblock_used(cblock); 
    if (f) { 
	Map *b = content_cblock_used(f,cblock); 
	flog("removing cache block %d (block %d of %d)",cblock,b->fblock,f->name);
	content_forget_cblock(f,cblock); 
	storage_forget(cblock);
	return TRUE; 
    } else { 
	flog("cache block %d not in use",cblock);
	return FALSE; 
    } 
} 

int next_cblock() { 
    return storage_next(); 
} 

/* remember sizes of files */ 
int remember_size(const char *name, int size) {
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    size_remember(name,size);
    return TRUE; 
} 
int get_size(const char *name) { 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return -1;
    } 
    return size_get(name); 
} 
int forget_size(const char *name) { 
    if (strlen(name)>255) { 
	flog("name %s too long",name); 
	return FALSE;
    } 
    size_forget(name); 
    return TRUE; 
} 

void print_all() { 
    storage_print(); 
    size_print(); 
    clist_print(); 
} 

#if 0
main()
{ 
    content_add("c");
    content_print(); 
    content_add("a");
    content_print(); 
    Content *f= content_add("b");
    map_add(&(f->bhead),2,4); 
    map_add(&(f->bhead),7,5); 
    map_add(&(f->bhead),3,2); 
    content_print(); 
    content_add("d");
    content_print(); 
} 
#endif

#if 0
char foo[BLOCKSIZE]; 

main()
{ 
    storage_begin(100); 
    store_block("foo",0,0,foo); 
    store_block("foo",1,1,foo); 
    store_block("foo",2,4,foo); 
    store_block("bar",0,4,foo); 
    store_block("bar",1,100,foo); 
    size_remember("foo",200); 
    size_remember("bar",300); 
    printf("size of %s is %d\n","foo",size("foo")); 
    storage_end(); 
} 
#endif 
