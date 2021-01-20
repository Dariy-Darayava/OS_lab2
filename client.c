#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

//define consts
#define MAX_MSG_LEN 80
#define DEFAULT_PORT 8080
#define DEFAULT_ADDRESS "127.0.0.1"
//define masks
#define CONFIG_FLAG_a 0x1
#define CONFIG_FLAG_p 0x2
#define CONFIG_FLAG_v 0x4
#define CONFIG_FLAG_V 0x8
#define CONFIG_FLAG_h 0x10

//structs
/////////////////////////////////////////////////structs
typedef struct
{
    unsigned long flags; //...000hVvpa
    char *address;//server address
    unsigned short port;//server port. Duh
}config;

//vars
char version[] = "1.3.1";
config conf = 
{
    0,//flags
    NULL,//address
    0//port
};

/////////////////////////////////////////////////macro
//this one for access conf.flags easily(conffX = config flag X)
#define conff(X) (conf.flags & CONFIG_FLAG_##X)
//this one for setting flags easily(sconffX = set config flag X)
#define sconff(X) (conf.flags |= CONFIG_FLAG_##X)

//functions
int setup(int argc, char *argv[]){//option parcing and env vars fetching
    //option parcing
    while (1)
    {
        
        int c = -1;//for getopt
        c = getopt(argc, argv, "-a:p:vVh");//read options
        if (c == -1)//if -1 we reached end of argv
            break;
        switch(c)
        {
            case 'a':
                if (conff(a)) 
                    return 1;
                sconff(a);
                conf.address = strdup(optarg);//copy address
                if (conf.address == NULL)
                    return 2;
                break;
            case 'p':
                if (conff(p))
                    return 3;
                sconff(p);
                if (sscanf(optarg, "%hu", &conf.port) != 1)
                    return 4;
                break;
            case 'v':
                if (conff(v)) 
                    return 5;
                sconff(v);
                break;
            case 'V':
                if (conff(V)) 
                    return 6;
                sconff(V);
                break;
            case 'h':
                if (conff(h)) 
                    return 7;
                sconff(h);
                break;
            case '?':
                return 8;
                break;
            case ':':
                return 9;
                break;
            default:
                return 10;
                break;

        }
    }
    //argv is now parced. Now we should check for any env vars
    char *env;
    if (!conff(a)){//-l not set -> check env vars
        env = getenv("L2ADDR");
        if (env){
            if (!(conf.address = strdup(env)))
                return 11;
        }
    }
    if (!conff(p)){//if -w was not set -> check env var
        env = getenv("L2PORT");
        if (env){
            if(sscanf(env,"%hu", &conf.port) != 1)
                return 12;
        }
            
    }
    

    return 0;
}

int print_version(){
    printf("Version:%s\n", version);
    return 0;
}

int print_help(){
    printf("The server stores numbers\n");
    printf("To add number use:ADD [num]\n");
    printf("To get number use:GET\n\n");
    printf("Available options:\n\n");
    
    printf("-a [address] - set ip address\n");
    printf("-p [num] - set port\n");
    printf("-v - show version\n");
    printf("-h - show help\n");
    
    printf("\nAvailable environment variables:\n");
    printf("L2ADDR - same as -a\n");
    printf("L2PORT - same as -p\n");
    printf("*Environment variables only checked if whort option is not found\n");
    
    return 0;
}


int main(int argc, char *argv[])
{    
    
    int rez;//test various func returns
    int client_socket;
    struct sockaddr_in server_addr = {0};
    
    char msg[MAX_MSG_LEN+1];
    //setup
    if (rez = setup(argc, argv)){
        printf("Setup error(%d)\n",rez);
        exit(EXIT_FAILURE);
    }
    
    if (conff(v))
        print_version();
    
    if(conff(h)){
        print_help();
        exit(EXIT_SUCCESS);
    }
    
    
    if (!(client_socket = socket(AF_INET, SOCK_DGRAM /*| SOCK_NONBLOCK*/, 0))){
        printf("Socket setup error\n");
        exit(EXIT_FAILURE);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = (conff(p))? htons(conf.port) : DEFAULT_PORT;
    if ((server_addr.sin_addr.s_addr = (conff(a))? inet_addr(conf.address): inet_addr(DEFAULT_ADDRESS)) == INADDR_NONE){
        printf("Unable to set setver addr\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Data sent:\n");
    fgets(msg, sizeof(msg), stdin);
    char *pos = strchr(msg, '\n');
    if (pos) *pos = '\0';
    rez = sendto(client_socket, msg, strlen(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (rez < 0){ printf("Sendto error\n"); exit(EXIT_FAILURE);}
    
    memset(msg, 0, sizeof(msg));
    
    printf("Waiting for server response...\n");
    
    rez = recvfrom(client_socket, msg, sizeof(msg), 0, NULL, NULL);
    if (rez < 0){ printf("Recvfrom error\n"); exit(EXIT_FAILURE);}
    
    printf("Server response:\n%s\n", msg);
    
    close(client_socket);
    
    return 0;
}
