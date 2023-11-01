CC = gcc

all: server app_handler

server: main.c Server.c LinkedList.c
	$(CC) -o main main.c Server.c LinkedList.c

app_handler: app_handler.c LinkedList.c
	$(CC) -o app_handler app_handler.c LinkedList.c -lpthread -lhiredis -levent

clean:
	rm -f main
	rm -f app_handler

