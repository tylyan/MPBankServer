CC = gcc
CFLAGS = -Wall -g -pthread

all: server servermm client

server: bankserver.c bankserver.h bankaccount.c
	$(CC) $(CFLAGS) -o server bankserver.c

servermm: bankservermm.c bankserver.h bankaccount.c
	$(CC) $(CFLAGS) -o servermm bankservermm.c

client: bankclient.c
	$(CC) $(CFLAGS) -o client bankclient.c

clean:
	rm server client servermm 
