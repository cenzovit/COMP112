all: squirrel nuts udpclient
squirrel: squirrel.o operations.o storage.o 
	gcc -g -o squirrel squirrel.o operations.o storage.o 
nuts: nuts.o
	gcc -g -o nuts nuts.o 

nuts.o: nuts.c
	gcc -g -c nuts.c

operations.o: operations.c storage.h 
	gcc -g -c operations.c

storage.o: storage.c storage.h
	gcc -g -c storage.c

squirrel.o: squirrel.c operations.h storage.h 
	gcc -g -c squirrel.c

udpclient: udpclient.o
	gcc -g -o udpclient udpclient.o 

udpclient.o: udpclient.c 
	gcc -g -c udpclient.c
