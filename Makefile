all:
	gcc -o lab2server server.c -lrt -pthread
	gcc -o lab2client client.c -lrt -pthread
