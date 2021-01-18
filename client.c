#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHECK_RESULT(res, msg) \
do { \
if (res < 0) { \
perror(msg); \
exit(EXIT_FAILURE); \
} \
} while (0)

#define BUF_SIZE 1024

int main() {
int clientSocket;
char buffer[BUF_SIZE] = {0};
struct sockaddr_in serverAddr = {0};
socklen_t addr_size;

clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
CHECK_RESULT(clientSocket, "socket");

serverAddr.sin_family = AF_INET;
serverAddr.sin_port = htons(5555);
serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

printf("Data sent: ");
fgets(buffer, sizeof(buffer), stdin);
char *pos = strchr(buffer, '\n');
if (pos) *pos = '\0';

int res = sendto(clientSocket, buffer, strlen(buffer), 0,
(const struct sockaddr*)&serverAddr, sizeof(serverAddr));
CHECK_RESULT(res, "sendto");

memset(buffer, 0, sizeof(buffer));

/*res = recvfrom(clientSocket, buffer, sizeof(buffer), 0, NULL, NULL);
CHECK_RESULT(res, "recvfrom");

printf("Data received (%d bytes): [%s]\n", (int)res, buffer);*/

close(clientSocket);

return 0;
}
