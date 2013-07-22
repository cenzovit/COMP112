CFLAGS= -g -O2

all: client server server.c help.c help.h help.o 

clean: 
	rm -rf client

client: client.c help.o help.h 
	gcc $(CFLAGS) client.c help.o -o client
	
# link files that might change

server: /comp/112/a/a2/server
	ln -s /comp/112/a/a2/server .
server.c: /comp/112/a/a2/server.c
	ln -s /comp/112/a/a2/server.c .
#help.c: /comp/112/a/a2/help.c
#	ln -s /comp/112/a/a2/help.c .
#help.h: /comp/112/a/a2/help.h
#	ln -s /comp/112/a/a2/help.h .
#help.o: /comp/112/a/a2/help.o
#	ln -s /comp/112/a/a2/help.o .
