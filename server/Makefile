CC = gcc

all: server app_handler

server: main.c Server.c
	$(CC) -o server main.c Server.c

app_handler: app_handler.c
	$(CC) -o app_handler app_handler.c -lpthread

clean:
	rm -f server
	rm -f app_handler

