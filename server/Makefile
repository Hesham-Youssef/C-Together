CC = gcc

all: main app_handler

main: main.c Server.c LinkedList.c Websocket.c
	$(CC) -o main main.c Server.c LinkedList.c Websocket.c -lssl -lcrypto

app_handler: app_handler.c LinkedList.c
	$(CC) -o app_handler app_handler.c LinkedList.c Websocket.c -lpthread -lhiredis -levent -lssl -lcrypto

clean:
	rm -f main
	rm -f app_handler

