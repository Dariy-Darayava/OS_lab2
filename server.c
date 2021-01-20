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

#define SHM_NAME "/l2shm"
#define SEM_NAME "/l2sem"
#define MAX_CLI_NUM 10
#define MAX_CLI_ENUM_SIZE 10
#define MAX_CLI_DATA MAX_CLI_ENUM_SIZE*MAX_CLI_NUM
#define SHM_SIZE 1024

#define PATH_MAX 120

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


typedef struct server_shared_data{
    unsigned long seccess_query_count;
    unsigned long error_query_count;
    int curr_number_of_clients;
    in_addr_t clients[MAX_CLI_NUM];
    unsigned int client_enum_size[MAX_CLI_NUM];
    double data[MAX_CLI_DATA];
    
}server_shared_data;

/////////////////////////////////////////////////vars
const char version[] = "1.3.0";


int server_socket;
char verbose_mode = 0;//additional output
char signal_flags = 0;

time_t start_time;
config conf = 
{
    0,//flags
    0,//wait
    NULL,//log
    NULL,//address
    0//port
};
FILE *logfd;
int shmfd;
struct server_shared_data *ssd;
int semfd;
sem_t *sem;
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
int setup(int argc, char *argv[]){//option parcing and env vars fetching
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
                if (sscanf(optarg, "%hu", &conf.port) != 1)
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
            if(sscanf(env,"%hu", &conf.port) != 1)
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
    
    srand(time(NULL));
    return 0;
}

void daemonize(){
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    chdir(cwd);

    /* Close all open file descriptors */    
    for (int i = sysconf(_SC_OPEN_MAX); i>=0; i--)
    {
        if (i != fileno(logfd))
            close (i);
    }
}

int lprintf(char *msg, ...){
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
    fflush(logfd);
    va_end(vlist);
    free(msgwt);
    return rez;
}

int print_version(){
    lprintf("Version:%s\n", version);
    if (!(conff(d)))
        printf("Version:%s\n", version);
    return 0;
}

int print_help(){
    printf("The server stores numbers\n");
    printf("To add number use:ADD [num]\n");
    printf("To get number use:GET\n\n");
    printf("Available options:\n\n");
    
    printf("-w [num] - imitate work by stopping child for num sec\n");
    printf("-d - start as daemon\n");
    printf("-l [path] - set log (/tmp/lab2.log - default file)\n");
    printf("-a [address] - set ip address\n");
    printf("-p [num] - set port\n");
    printf("-v - show version\n");
    printf("-h - show help\n");
    
    printf("\nAvailable environment variables:\n");
    printf("L2WAIT - same as -w\n");
    printf("L2LOGFILE - same as -l\n");
    printf("L2ADDR - same as -a\n");
    printf("L2PORT - same as -p\n");
    printf("*Environment variables only checked if whort option is not found\n");
    
    return 0;
}


int create_and_configure_storage(){
    if ((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG)) < 0)
        return 1;
    if (ftruncate(shmfd,SHM_SIZE))
        return 2;
    ssd = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (ssd == MAP_FAILED)
        return 3;
    memset(ssd,0, SHM_SIZE);
    
    
    if ((semfd = shm_open(SEM_NAME, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG)) < 0)
        return 4;
    if (ftruncate(semfd,sizeof(sem_t)))
        return 5;
    sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, semfd, 0);
    if (sem == MAP_FAILED)
        return 6;
    sem_init(sem, 1, 1);
    
    return 0;
}

int add_to_storage(double *num, struct sockaddr_in *client_addr){
    sem_wait(sem);
    if (conff(s)){
        //if we have individual enum for each client
        int curr_client_index = -1;
        for(int i = 0; i < MAX_CLI_NUM; i++)//find our client number
            if (ssd->clients[i] == client_addr->sin_addr.s_addr){
                curr_client_index = i;
                break;
            }
        if (curr_client_index == -1){//if we work with this client first time
            if (ssd->curr_number_of_clients == MAX_CLI_NUM){
                //we have no space for new user;
                ssd->error_query_count++;
                sem_post(sem);
                return 1;
            }
            ssd->clients[ssd->curr_number_of_clients] = client_addr->sin_addr.s_addr;
            ssd->data[ssd->curr_number_of_clients*MAX_CLI_ENUM_SIZE] = *num;
            ssd->client_enum_size[ssd->curr_number_of_clients] = 1;
            ssd->seccess_query_count++;
            sem_post(sem);
            return 0;
            
        }else{
            if (ssd->client_enum_size[curr_client_index] == MAX_CLI_ENUM_SIZE){
                //this clients enum is full
                ssd->error_query_count++;
                sem_post(sem);
                return 2;
            }
            ssd->data[curr_client_index*MAX_CLI_ENUM_SIZE + ssd->client_enum_size[curr_client_index]] = *num;
            ssd->client_enum_size[curr_client_index]++;
            ssd->seccess_query_count++;
            sem_post(sem);
            return 0;
        }
            
            
        
    }else{
        //if we have one storage for everyone;
        if(ssd->client_enum_size[0] == MAX_CLI_DATA){
            //we have no space for new data
            ssd->error_query_count++;
            sem_post(sem);
            return 3;
        }
        ssd->data[ssd->client_enum_size[0]] = *num;
        ssd->client_enum_size[0]++;
        ssd->seccess_query_count++;
        sem_post(sem);
        return 0;
    }
}

int get_from_storage(double *num, struct sockaddr_in *client_addr){
    sem_wait(sem);
    if (conff(s)){
        //separate storage for everyone
        int curr_client_index = -1;
        for(int i = 0; i < MAX_CLI_NUM; i++)//find our client number
            if (ssd->clients[i] == client_addr->sin_addr.s_addr){
                curr_client_index = i;
                break;
            }
        if (curr_client_index == -1){
            //no data to get for this client
            ssd->error_query_count++;
            sem_post(sem);
            return 1;
        }
        
        if(ssd->client_enum_size[curr_client_index] == 0){
            //no items for this client
            ssd->error_query_count++;
            sem_post(sem);
            return 2;
        }
        
        if (ssd->client_enum_size[curr_client_index] == 1){
            //last item for this client
            *num = ssd->data[curr_client_index*MAX_CLI_ENUM_SIZE];
            ssd->data[curr_client_index*MAX_CLI_ENUM_SIZE] = 0;
            ssd->client_enum_size[curr_client_index] = 0;
            ssd->seccess_query_count++;
            sem_post(sem);
            return 0;
        }else{
            //not the last item
            int removed_num_index = rand() % ssd->client_enum_size[curr_client_index];
            *num = ssd->data[curr_client_index*MAX_CLI_ENUM_SIZE + removed_num_index];
            //shift for 1 double
            memmove(ssd->data + curr_client_index*MAX_CLI_ENUM_SIZE + removed_num_index, ssd->data + curr_client_index*MAX_CLI_ENUM_SIZE + removed_num_index + 1, sizeof(double
            )*(ssd->client_enum_size[curr_client_index] - removed_num_index - 1));
            
            ssd->data[curr_client_index*MAX_CLI_ENUM_SIZE + ssd->client_enum_size[curr_client_index] - 1] = 0.0;
            
            ssd->client_enum_size[curr_client_index]--;
            ssd->seccess_query_count++;
            sem_post(sem);
            return 0;
        }
    }else{
        //we have one enum
        if(ssd->client_enum_size[0] == 0){
            //we have no data
            ssd->error_query_count++;
            sem_post(sem);
            return 3;
        }
        if(ssd->client_enum_size[0] == 1){
            *num = ssd->data[0];
            ssd->client_enum_size[0] = 0;
            ssd->seccess_query_count++;
            sem_post(sem);
            return 0;
        }else{
            int removed_num_index = rand() % ssd->client_enum_size[0];
            *num = ssd->data[removed_num_index];
            
            memmove(ssd->data + removed_num_index, ssd->data + removed_num_index + 1, sizeof(double)*(ssd->client_enum_size[0] - removed_num_index - 1));
            
            ssd->data[ssd->client_enum_size[0] - 1] = 0.0;
            ssd->client_enum_size[0]--;
            ssd->seccess_query_count++;
            sem_post(sem);
            return 0;
        }
    }
    return 0;
}

int configure_socket(){
    struct sockaddr_in addr = {0};
    if (!(server_socket = socket(AF_INET, SOCK_DGRAM /*| SOCK_NONBLOCK*/, 0)))
        return 1;
    
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        return 2;
        
    
    addr.sin_family = AF_INET;
    addr.sin_port = (conff(p))? htons(conf.port) : DEFAULT_PORT;
    addr.sin_addr.s_addr = (conff(a))? inet_addr(conf.address): INADDR_ANY; 
        
    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return 3;
    return 0;
}

int create_task(list_action act, struct sockaddr_in client_addr, double num){
    int pid = fork();
    int rez;
    if (pid < 0)
        return pid;
    if (pid > 0)
        return 0;// exit create_task in child;
   
    
    //sendto(server_socket, "GOT", strlen("GOT"),0, (struct sockaddr*)&client_addr, sizeof(client_addr));
    
    
    
    
    if (conff(w))
        sleep(conf.wait);
    if (act == ADD){
/////////////// add number
        if (rez = add_to_storage(&num, &client_addr)){
            //error uccured
            lprintf("Child failed to add a number to storage(%d)\n", rez);
            
            rez = sendto(server_socket, "ERROR 1", strlen("ERROR 1"),0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            
            if (rez < 0)
                lprintf("Child failed to respond\n");
            _Exit(EXIT_FAILURE);
        }
        else{
            //Send OK
            rez = sendto(server_socket, "OK", strlen("OK"),0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            
            if (rez < 0)
                lprintf("Child failed to respond\n");
            _Exit(EXIT_SUCCESS);
        }
    }else{
/////////////// get number;
        if (get_from_storage(&num, &client_addr)){
            //error
            lprintf("Child failed to add a number to storage(%d)\n", rez);
            
            rez = sendto(server_socket, "ERROR 2", strlen("ERROR 2"),0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            
            if (rez < 0)
                lprintf("Child failed to respond\n");
            _Exit(EXIT_FAILURE);
        }
        else{
            //send num
            char msg[MAX_MSG_LEN] = {0};
            sprintf(msg, "%lf", num);
            rez = sendto(server_socket, msg, strlen(msg),0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            
            if (rez < 0)
                lprintf("Child failed to respond\n");
            _Exit(EXIT_SUCCESS);
        }
    }
    
}

int handle_received_msg(char *msg, struct sockaddr_in *client_addr){
    //valid messages are:
    //ADD [num]
    //GET
    if (strncmp(msg, "GET", 3) == 0){
        if (strlen(msg) != 3)
            return 2;
        return create_task(GET, *client_addr, 0);
    }else{
        char *ptr; char endcheck;
        char *tmpmsg;
        double number;
        
        if (!(tmpmsg = strdup(msg)))
            return 3;
        if (!(ptr = strtok(tmpmsg, " ")))
            {free(tmpmsg); return 4;}
        if (strncmp(ptr, "ADD", 3) != 0)
            {free(tmpmsg); return 5;}
        if (!(ptr = strtok(NULL, " ")))
            {free(tmpmsg); return 6;}
        if (sscanf(ptr, "%lf%c", &number, &endcheck) != 1)
            {free(tmpmsg); return 7;}
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

int cleanup(){
    if(conf.log != NULL)
        free(conf.log);
    if(conf.address != NULL)
        free(conf.address);
    return 0;
}

int handle_signals(){
    if (sigf(INT)){
        rsigf(INT);
        cleanup();
        exit(EXIT_SUCCESS);
    }
    if (sigf(TERM)){
        rsigf(TERM);
        cleanup();
        exit(EXIT_SUCCESS);
    }
    if (sigf(QUIT)){
        rsigf(QUIT);
        cleanup();
        exit(EXIT_SUCCESS);
    }
    if (sigf(USR1)){
        rsigf(USR1);
        time_t work_time = time(NULL) - start_time;
        struct tm work_time_tm = *localtime(&work_time);
    
        sem_wait(sem);
        lprintf("Showing statistics:\nWorking for:[%02d.%02d.%d %02d:%02d:%02d]\nSuccess requests:%d\nError requests:%d\n", work_time_tm.tm_mday, work_time_tm.tm_mon + 1,  work_time_tm.tm_year, work_time_tm.tm_hour, work_time_tm.tm_min, work_time_tm.tm_sec, ssd->seccess_query_count, ssd->error_query_count);
        
        if (!(conff(d)))
            fprintf(stderr, "Showing statistics:\nWorking for:[%02d.%02d.%d %02d:%02d:%02d]\nSuccess requests:%d\nError requests:%d\n", work_time_tm.tm_mday, work_time_tm.tm_mon + 1,  work_time_tm.tm_year, work_time_tm.tm_hour, work_time_tm.tm_min, work_time_tm.tm_sec, ssd->seccess_query_count, ssd->error_query_count);     
        
        sem_post(sem);
    }
    
        
    return 0;
}


//main. Duh
int main(int argc, char *argv[])
{    
    start_time = time(NULL);
    
    //printf("%02d:%02d:%02d\n", c_time.tm_hour, c_time.tm_min, c_time.tm_sec);*/
    
    
    
    int rez;//test various func returns
    
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
        
    if (conff(d)){
        daemonize();
    }
    
    if (rez = create_and_configure_storage()){
        lprintf("Storage setup error(%d):%s\n",rez, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (rez = configure_socket()){
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
                        sendto(server_socket, "ERROR 1", strlen("ERROR 1"),0, (struct sockaddr*)&curr_client_addr, curr_client_addr_size);
                        sem_wait(sem);
                        ssd->error_query_count++;
                        sem_post(sem);
                    }else{
                        lprintf("Msg {%s} was handled.\n", recvmsg);
                    }
                        
                }
                
            }
        }
            
                
        
        if (signal_flags)// signal_flags != 0 if at least one flag is 1
            handle_signals();
    }
    
    
    cleanup();
    return 0;
}
