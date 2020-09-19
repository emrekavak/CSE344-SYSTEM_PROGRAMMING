all: server client
server: server.c
	gcc -Wall -g server.c -o server -lpthread -lrt
client: client.c
	gcc -Wall -g client.c -o client -lpthread -lrt
clean:
	rm server client