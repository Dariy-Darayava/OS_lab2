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

//define consts

//define masks
#define CONFIG_FLAG_w 0x1
#define CONFIG_FLAG_d 0x2
#define CONFIG_FLAG_l 0x4
#define CONFIG_FLAG_a 0x8
#define CONFIG_FLAG_p 0x10
#define CONFIG_FLAG_v 0x20
#define CONFIG_FLAG_V 0x40
#define CONFIG_FLAG_h 0x80
#define CONFIG_FLAG_s 0x100

#define SIGNAL_FLAG_INT 0x1
#define SIGNAL_FLAG_TERM 0x2
#define SIGNAL_FLAG_QUIT 0x4
#define SIGNAL_FLAG_USR1 0x8

#define DEFAULT_PORT 8080
//default addr -> INADDR_ANY

#define MAX_MSG_LEN 80


/////////////////////////////////////////////////structs
typedef struct
{
    unsigned long flags; //...000shVvpaldw
    unsigned int wait;//imitate usefull
    char *log;//long file name form env or -l(/tmp/lab2.log by default)
    char *address;//server address
    unsigned short port;//server port. Duh
}config;

typedef enum list_action{
    ADD,
    GET
}list_action;

typedef struct list_member{
    double data;
    struct list_member *next;
}list_member;

typedef struct list{
    unsigned int count;
    list_member *first;
    list_member *last;
}list;


typedef struct server_statistics{
    unsigned long seccess_query_count;
    unsigned long error_query_count;
}server_statistics;

/////////////////////////////////////////////////vars
char verbose_mode = 0;//additional output
char signal_flags = 0;

config conf = 
{
    0,//flags
    0,//wait
    NULL,//log
    NULL,//address
    0//port
};
FILE *logfd;
    
/////////////////////////////////////////////////macro
//this one for access conf.flags easily(conffX = config flag X)
#define conff(X) (conf.flags & CONFIG_FLAG_##X)
//this one for setting flags easily(sconffX = set config flag X)
#define sconff(X) (conf.flags |= CONFIG_FLAG_##X)

//get signal flag
#define sigf(X) (signal_flags & SIGNAL_FLAG_##X)
//set signal flag
#define ssigf(X) (signal_flags |= SIGNAL_FLAG_##X)
//remove signal flag
#define rsigf(X) (signal_flags &= ~SIGNAL_FLAG_##X)

/////////////////////////////////////////////////functions
int setup(int argc, char *argv[])//option parcing and env vars fetching
{
    //option parcing
    while (1)
    {
        
        int c = -1;//for getopt
        c = getopt(argc, argv, "-:w:dl:a:p:vVhs");//read options
        if (c == -1)//if -1 we reached end of argv
            break;
        switch(c)
        {
            case 'w':
                if (conff(w))//if we meet it twice 
                    return 1;
                sconff(w);
                if (sscanf(optarg, "%u", &conf.wait) != 1)
                    return 2;
                break;
            case 'd':
                if (conff(d)) 
                    return 3;
                sconff(d);
                break;
            case 'l':
                if (conff(l)) 
                    return 4;
                sconff(l);
                conf.log = strdup(optarg);//copy log
                if (conf.log == NULL)
                    return 5;
                break;
            case 'a':
                if (conff(a)) 
                    return 6;
                sconff(a);
                conf.address = strdup(optarg);//copy address
                if (conf.address == NULL)
                    return 7;
                break;
            case 'p':
                if (conff(p))
                    return 8;
                sconff(p);
                if (sscanf(optarg, "%u", &conf.port) != 1)
                    return 9;
                break;
            case 'v':
                if (conff(v)) 
                    return 10;
                sconff(v);
                break;
            case 'V':
                if (conff(V)) 
                    return 11;
                sconff(V);
                break;
            case 'h':
                if (conff(h)) 
                    return 12;
                sconff(h);
                break;
            case 's':
                if (conff(s)) 
                    return 13;
                sconff(s);
                break;
            case '?':
                return 14;
                break;
            case ':':
                return 15;
                break;
            default:
                return 16;
                break;

        }
    }
    //argv is now parced. Now we should check for any env vars
    char *env;
    if (!conff(w)){//if -w was not set -> check env var
        env = getenv("L2WAIT");
        if (env){
            if(sscanf(env,"%u", &conf.wait) != 1)
                return 17;
        }
            
    }
    if (!conff(l)){//-l not set -> check env vars
        env = getenv("L2LOGFILE");
        if (env){
            conf.log = strdup(env);
        }
    }
    if (!conff(a)){//-l not set -> check env vars
        env = getenv("L2ADDR");
        if (env){
            conf.address = strdup(env);
        }
    }
    if (!conff(p)){//if -w was not set -> check env var
        env = getenv("L2PORT");
        if (env){
            if(sscanf(env,"%u", &conf.port) != 1)
                return 18;
        }
            
    }
    
    if (conf.log){//if log was set conf.log != null
        if (!(logfd = fopen(conf.log, "w+")))//if logfd == null
            return 19;
    }
    else{
        if (!(logfd = fopen("/tmp/lab2.log", "w+")))//if logfd == null
            return 20;
    }
    return 0;
}

int lprintf(char *msg, ...)
{
    char *msgwt;// message + time
    if (!(msgwt = malloc(22 + strlen(msg) + 1)))
        return -1;
    
    time_t raw_time = time(NULL);
    struct tm loc_time = *localtime(&raw_time);
    //add time to str
    sprintf(msgwt,"[%02d.%02d.%d %02d:%02d:%02d] ", loc_time.tm_mday, loc_time.tm_mon + 1, loc_time.tm_year + 1900, loc_time.tm_hour, loc_time.tm_min, loc_time.tm_sec);
    
    //add msg
    strcat(msgwt, msg);
    
    va_list vlist;
    va_start(vlist, msg);
    //write to log
    int rez = vfprintf(logfd, msgwt, vlist);
    va_end(vlist);
    free(msgwt);
    return rez;
}

int configure_socket(int *fd)
{
    struct sockaddr_in addr = {0};
    if (!(*fd = socket(AF_INET, SOCK_DGRAM /*| SOCK_NONBLOCK*/, 0)))
        return 1;
    
    addr.sin_family = AF_INET;
    addr.sin_port = (conff(p))? htons(conf.port) : DEFAULT_PORT;
    addr.sin_addr.s_addr = (conff(a))? inet_addr(conf.address): INADDR_ANY; 
        
    if (bind(*fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return 2;
    return 0;
}

int create_task(list_action act, struct sockaddr_in client_addr,...){
    printf("Task %d created\n", act);
    return 0;
}

int handle_received_msg(char *msg, struct sockaddr_in *client_addr){
    //valid messages are:
    //ADD [num]
    //GET
    if (strncmp(msg, "GET", 3) == 0){
        return create_task(GET, *client_addr);
    }else{
        char *ptr; char endcheck;
        char *tmpmsg;
        double number;
        
        if (!(tmpmsg = strdup(msg)))
            return 2;
        
        
        if (!(ptr = strtok(tmpmsg, " ")))
            {free(tmpmsg); return 3;}
        if (strncmp(ptr, "ADD", 3) != 0)
            {free(tmpmsg); return 4;}
        if (!(ptr = strtok(NULL, " ")))
            {free(tmpmsg); return 5;}
        if (sscanf(ptr, "%lf%c", &number, &endcheck) != 1)
            {free(tmpmsg); return 6;}
        free(tmpmsg);                    
        return create_task(ADD, *client_addr, number);
        
    }
        
    return 1;
}

void SIGINT_handler(int num){
    ssigf(INT);
}

void SIGTERM_handler(int num){
    ssigf(TERM);
}

void SIGQUIT_handler(int num){
    ssigf(QUIT);
}

void SIGUSR1_handler(int num){
    ssigf(USR1);
}

int setup_signal_handlers(){
    struct sigaction INT_action = {0}, TERM_action = {0}, QUIT_action = {0}, USR1_action = {0};
    //sigset_t INT_set, TERM_set, QUIT_set, USR1_set;
    
    /*sigemptyset(&INT_set);
    sigemptyset(&TERM_set);
    sigemptyset(&QUIT_set);
    sigemptyset(&USR1_set);*/
    
    INT_action.sa_handler = SIGINT_handler;
    TERM_action.sa_handler = SIGTERM_handler;
    QUIT_action.sa_handler = SIGQUIT_handler;
    USR1_action.sa_handler = SIGUSR1_handler;
    
    /*INT_action.sa_mask = INT_set;
    TERM_action.sa_mask = TERM_set;
    QUIT_action.sa_mask = QUIT_set;
    USR1_action.sa_mask = USR1_set;*/
    
    if (sigaction(SIGINT, &INT_action, 0))
        return 1;
    if (sigaction(SIGTERM, &TERM_action, 0))
        return 2;
    if (sigaction(SIGQUIT, &QUIT_action, 0))
        return 3;
    if (sigaction(SIGUSR1, &USR1_action, 0))
        return 4;
    
    return 0;
}

int handle_signals()
{
    if (sigf(INT)){
        rsigf(INT);
        exit(EXIT_SUCCESS);
    }
    if (sigf(TERM)){
        rsigf(TERM);
    }
    if (sigf(QUIT)){
        rsigf(QUIT);
    }
    if (sigf(USR1)){
        rsigf(USR1);
    }
    
        
    return 0;
}

int cleanup()
{
    if(conf.log != NULL)
        free(conf.log);
    if(conf.address != NULL)
        free(conf.address);
    return 0;
}

//main. Duh
int main(int argc, char *argv[])
{    
    /*time_t start_time = time(NULL);
    sleep(2);
    time_t curr_time = time(NULL);
    curr_time -= start_time;
    struct tm c_time = *localtime(&curr_time);
    printf("%02d:%02d:%02d\n", c_time.tm_hour, c_time.tm_min, c_time.tm_sec);*/
    
    
    
    int rez;//test various func returns
    int server_socket;
    //setup
    if (rez = setup(argc, argv)){
        printf("Setup error(%d)\n",rez);
        exit(EXIT_FAILURE);
    }
    if (rez = configure_socket(&server_socket)){
        lprintf("Unable to configure socket(%d)\n", rez);
        exit(EXIT_FAILURE);
    }
    
    if (rez = setup_signal_handlers()){
        lprintf("Unable to setup signal handlers(%d)\n", rez);
        exit(EXIT_FAILURE);
    }        
    
    int recvmsglen;
    char recvmsg[MAX_MSG_LEN+1];
    struct sockaddr_in curr_client_addr = {0};
    int curr_client_addr_size = sizeof(curr_client_addr);
    
    struct pollfd poll_scope[1];
    poll_scope[0].fd = server_socket;
    poll_scope[0].events = POLLIN;
    
    while(1){
        rez = poll(poll_scope, 1, 100);
        
        if (rez == -1)
            lprintf("Poll error(%d):%s\n", errno, strerror(errno));
        else if (rez > 0){
            //if sock is ready
            if (poll_scope[0].revents & POLLIN){
                poll_scope[0].revents = 0;
                
                memset(recvmsg, 0, MAX_MSG_LEN+1);
                memset(&curr_client_addr, 0, curr_client_addr_size);
                
                recvmsglen = recvfrom(server_socket, recvmsg, sizeof(recvmsg), 0, (struct sockaddr*)&curr_client_addr, &curr_client_addr_size);

                if (recvmsglen > 0){
                    rez = handle_received_msg(recvmsg, &curr_client_addr);
                    if (rez){
                        lprintf("Msg {%s} was not handled. Error code(%d)\n", recvmsg, rez);
                    }else{
                        lprintf("Msg {%s} was handled.\n", recvmsg);
                    }
                        
                }
                
            }
        }
        /*recvmsglen = recvfrom(server_socket, recvmsg, sizeof(recvmsg), 0, (struct sockaddr*)&curr_client_addr, &curr_client_addr_size);

        if (recvmsglen > 0){
            rez = handle_received_msg(recvmsg, &curr_client_addr);
            if (rez){
                lprintf("Msg {%s} was not handled. Error code(%d)\n", 2, recvmsg, rez);
            }else{
                lprintf("Msg {%s} was handled.\n", 1, recvmsg);
            }
                
        }*/
            
                
        
        if (signal_flags)// signal_flags != 0 if at least one flag is 1
            handle_signals();
    }
    
    
    cleanup();
    return 0;
}
