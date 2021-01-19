all:
	gcc -o server server.c -lrt -pthread
	gcc -o client client.c -lrt -pthread
