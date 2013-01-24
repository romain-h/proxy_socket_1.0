CC=gcc
all: proxy.c
	$(CC) -o proxy proxy.c

clean: 
	$(RM) count *.o *~
