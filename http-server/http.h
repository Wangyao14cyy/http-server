#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>
#include <sys/epoll.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h> 
#include <pthread.h>
#include <netdb.h>

#ifndef HTTP_SERVER_H__
#define HTTP_SERVER_H__

#define DEFAULTPORT 8899
#define DEFAULTLOG "/home/wangyao/log/http.log"
#define DEFAULTBACKLOG 10 //why 10?
#define DEFAULTDIR "/home/wangyao/http"

#define HOSTLEN 256
#define MAXBUF 1024
#define MAXEVENTS 10000

void derrormsg(char *msg);  //send the error message in the backgroud
void errormsg(char *msg);   //send the error message not in the backgroud
void dinfomsg(char *msg);	  //send the proper message in the backgroud 	
void infomsg(char *msg);    //send the proper message not in the backgroud

int make_socket(int sockfd);          //The sockfd is made to socket,bind,listen and return 
char *get_defaultip();				  //If you didn't choose a ip-address,it's will get the default ip-address 

int sendall(int fd,char *buf,int *len,int judge);        //remember turn to next line by "\n";
void file_not_found(int clifd,char *args);
void daytime(void);//report time when you start the server!!!!
int openssl(int sockfd);
void errorfunc(char *msg);
void infofunc(char *msg);
void ssl_init(SSL_CTX *ctx);
void setnonblock(int fd);
void addfd(int epollfd,int fd);
void lt(struct epoll_event *events,int number,int epollfd,int listenfd,int listenfds);//switch to LT mode
void getinfomation(int clifd,int judge);
void echo_command(int clifd,char *command,char *argument,char *buf,int judge);
void Get(int clifd,char *argument,int judge);
void Head(int clifd,char *argument,int judge);
void Options(int clifd,int judge);
void Post(int clifd,char *argument,char *buf,int judge); 
char *file_type(char *argument);
void sendmsgs(int clifd,char *type,char *argument,int length,int judge);
void sendhead(int clifd,char *type,int length,int err_bit,int judge);
void senderror(int clifd,int bit,int judge);

void tpool_destroy();
int tpool_create(int thr_num);
int tpool_add_work(void *(*routine)(void *),void *arg);
void* thread_routine(void *arg);
void *func1(void *arg);
void *func2(void *arg);
void *func3(void *arg);
int make_socket_ssl(int sockfds);

int ssl_number=0;
char buf[MAXBUF+1];
char *ip=NULL;
char *_log=NULL;  
char *dirroot=NULL;
int port=0;
int daemon_check=0; // -D option
int logfd=0;  
char *errno_q=NULL;
char *req[3]={"200 OK","500 Internal Server Error","404 Not Found"};
typedef struct tpool_work
{
    void* (*routine)(void *);
    void *arg;
    struct tpool_work *next;   
}tpool_work_t;

typedef struct
{
    int shutdown;  //close the pthread pool if the value is 1
    int max_thr_num;
    pthread_t *thr_id;
    tpool_work_t *queue_head;       
    pthread_cond_t queue_ready;      
    pthread_mutex_t queue_lock;
}tpool_t;

typedef struct
{
    int clifd;
    int type;
}sign;

typedef struct
{
    int listenfd;
    int epollfd;
}sign2;

static int tlsfd[1000];
static tpool_t *tpool=NULL;

pthread_mutex_t mutex_all; 

void* thread_routine(void *arg)
{
    tpool_work_t *work;
    while(1)
    {
        pthread_mutex_lock(&tpool->queue_lock);
        while(!tpool->queue_head&&!tpool->shutdown)
        {
            pthread_cond_wait(&tpool->queue_ready,&tpool->queue_lock);
        }
        if(tpool->shutdown)
        {
            pthread_mutex_unlock(&tpool->queue_lock);
            pthread_exit(NULL);
        } 
        work=tpool->queue_head;
        tpool->queue_head=tpool->queue_head->next;
        pthread_mutex_unlock(&tpool->queue_lock);
        
        work->routine(work->arg);
        free(work);
    }   
    return NULL;    
}



void daemons(void)
{		
    umask(0);
	if(0!=fork())
		exit(0);
	
	if(-1==setsid())
		derrormsg("setsid error!");
	
	if(0!=fork())
		exit(0);
	
	chdir("/");		
	
	close(STDOUT_FILENO);
	close(STDIN_FILENO);
	close(STDERR_FILENO);
	
	int fd=open("/dev/null",O_RDONLY);
	dup2(fd,STDOUT_FILENO);
	dup2(fd,STDERR_FILENO);
}

void daytime(void)
{
    int lengths;
	int ret;
	struct sockaddr_in cli_address;
	int sockfd;
	char recvnum[MAXBUF];
	bzero(recvnum,MAXBUF);
	bzero(&cli_address,sizeof(struct sockaddr_in));
	if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)	
		errormsg("when daytime socket error!");
	if(inet_pton(AF_INET,"127.0.0.1",(void *)&cli_address.sin_addr)<=0)
		errormsg("when daytime get ip-localhost error!");
	cli_address.sin_port=htons(13);
	cli_address.sin_family=AF_INET;
	
	if(-1==connect(sockfd,(struct sockaddr *)&cli_address,sizeof(struct sockaddr_in)))
		errormsg("when daytime connect error!");
	int n=0;
	while((n=read(sockfd,recvnum,MAXBUF))>0)
		{
            lengths=strlen(recvnum);
			sendall(logfd,recvnum,&lengths,0);
            lengths=strlen(recvnum);
			sendall(STDOUT_FILENO,recvnum,&lengths,0);
		}	
	if(n<0)
		errormsg("when daytime output error!");
	close(sockfd);
}

int sendall(int fd,char *buf,int *len,int judge) 
{
	int n;
	int total=0;
	int byteleft=(*len);
	while(total<(*len))
	{	
		n=send(fd,buf+total,byteleft,0);
		if(-1==n)
			break;  //There is not call errorlog code,because it's will call abort().But the server can't stop in here.
		byteleft-=n;
		total+=n;
	}
	len=&total; //return the number you write to buffer
	return -1==n?-1:0;
}

void derrormsg(char *msg)
{
    int lengths;
	errno_q=strerror(errno);
	lengths=strlen(errno_q);
    sendall(logfd,errno_q,&lengths,0);
    lengths=strlen(msg);
	sendall(logfd,msg,&lengths,0);
	abort();
}

void errormsg(char *msg)
{
    int lengths;
	errno_q=strerror(errno);
    lengths=strlen(errno_q);
	sendall(logfd,errno_q,&lengths,0);
    lengths=strlen(msg);
	sendall(logfd,msg,&lengths,0);
    lengths=strlen(errno_q);
	sendall(STDOUT_FILENO,errno_q,&lengths,0);
    lengths=strlen(msg);
	sendall(STDOUT_FILENO,msg,&lengths,0);
	abort();
}

void infomsg(char *msg)
{
    int lengths;
    lengths=strlen(msg);
	sendall(logfd,msg,&lengths,0);
    lengths=strlen(msg);
	sendall(STDOUT_FILENO,msg,&lengths,0);
}

void dinfomsg(char *msg)
{
    int lengths;
    lengths=strlen(msg);
	sendall(logfd,msg,&lengths,0);
}

int make_socket(int sockfd)
{
	struct sockaddr_in servaddr;
	bzero(&servaddr,sizeof(struct sockaddr_in));
	if((sockfd=socket(AF_INET,SOCK_STREAM,0)<0))
		errorfunc("socket error!");	
	if(0==port)
		port=DEFAULTPORT;
	if(NULL==ip)
	    char *ip_temp=get_defaultip();
	else
        char *ip_temp=ip;
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(port);
	
	if(1!=inet_pton(AF_INET,ip_temp,(void *)&servaddr))
		errorfunc("inet_pton error!");
	if(-1==bind(sockfd,(struct sockaddr *)&servaddr,sizeof(struct sockaddr_in)))
		errorfunc("bind error!");	
	if(-1==listen(sockfd,DEFAULTBACKLOG))
		errorfunc("listen error!");
	
	return sockfd;				
}

char *get_defaultip()
{
	int namelen;	
	char *hostname=NULL;
	char **pptr=NULL; // maybe dangerous
	char *ipaddr=NULL;
	if(-1==gethostname(hostname,namelen))
		errorfunc("gethostname error!");
	struct hostent *addrinfo;			
	if(NULL==(addrinfo=gethostbyname(hostname)))
		errorfunc("gethostbyname error!");	
	switch(addrinfo->h_addrtype){
		case AF_INET:
			ipaddr=*(addrinfo->	h_addr_list);
			break;
		default:
			errorfunc("h_addr_list error");		
			break;	
		}
	return ipaddr;	
}

void errorfunc(char *msg)
{
		if(daemon_check)
			derrormsg(msg);
		else 
			errormsg(msg);
}

void infofunc(char *msg)
{
	if(daemon_check)
		dinfomsg(msg);
	else
		infomsg(msg);
}

void getoption(int argc,char **argv)  // hard to know how to use getopt() 
{
	int c=0;
	struct option long_options[]={
		{"ip",1,NULL,'I'},
		{"log",1,NULL,'L'},
		{"port",1,NULL,'P'},
		{"dirroot",1,NULL,'D'},
		{"daemon",0,NULL,'B'},
		{0,0,0,0},
		};
	char *l_opt_arg=NULL;
	char *const short_options="I:L:P:D:B";	
	while((c=getopt_long(argc,argv,short_options,long_options,NULL))!=-1)
		switch(c)
		{
			case 'I':
				l_opt_arg=optarg;
				ip=l_opt_arg;
				break;
			case 'L':
				l_opt_arg=optarg;
				_log=l_opt_arg;
				break;
			case 'P':
				l_opt_arg=optarg;
				port=atoi(l_opt_arg);	
				break;
			case 'B':
				daemon_check=1;
				break;
			case 'D':
				l_opt_arg=optarg;
				dirroot=l_opt_arg;	
				break;
		}
}

void setnonblock(int fd)
{
	int old_option=fcntl(fd,F_GETFL);
	if(old_option==-1)
		errorfunc("fcntl error!");
	int new_option=old_option | O_NONBLOCK;
	if(-1==fcntl(fd,F_SETFL,new_option))
			errorfunc("fcntl error!");
}

void addfd(int epollfd,int fd)
{
	struct epoll_event event;
	event.data.fd=fd;
	event.events=EPOLLIN;
	if(-1==epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event))
		errorfunc("epollctl error!");
	setnonblock(fd);
}

void lt(struct epoll_event *events,int number,int epollfd,int listenfd,int listenfds)
{
    int judge=0;
	for(int i=0;i<number;i++)
	{
		int sockfd=events[i].data.fd;
		if(sockfd==listenfd)
		{
            sign2 *arg=NULL;
            arg=(sign2 *)malloc(sizeof(sign2));
            arg->listenfd=listenfd;
            arg->epollfd=epollfd;
            tpool_add_work(func2,(void *)arg);
		}
        else if(sockfd==listenfds)
        {
            sign2 *arg=NULL;//ssl code loop
            arg=(sign2 *)malloc(sizeof(sign2));
            arg->listenfd=listenfds;
            arg->epollfd=epollfd;
            tpool_add_work(func3,(void *)arg);
        }
		else if(events[i].events & EPOLLIN)
        {
            for(int t=0;t<ssl_number;t++)
                if(sockfd==tlsfd[t])
                {
                    judge=1;
                    break;
                }
            sign *arg=NULL;
            arg=(sign *)malloc(sizeof(sign));
            arg->type=judge;
            arg->clifd=sockfd;
            tpool_add_work(func1,(void *)arg);
        }        
        else
			infofunc("something else happened!");
	}
}

void getinfomation(int clifd,int judge)
{
	char buf[MAXBUF];
	int recvnum;
	char command[10];
    char argument[MAXBUF];
	char mes[MAXBUF];
	bzero(buf,MAXBUF);
	bzero(argument,MAXBUF);
	bzero(command,10);
	bzero(mes,MAXBUF);
	
    strcpy(argument,"./");
    
    if(0>(recvnum=recv(clifd,buf,sizeof(buf),0)))
		{
            infofunc("POST error!");
            return;
        }    
	if(2==sscanf(buf,"%s /%s",command,argument+2))//when %s meet a blank ,it will stop
		{
			sprintf(mes,"recv data:\n%s",buf);
			infomsg(mes);
		}	
	else
        bzero(command,10);
		strcpy(command,"ERROR");
        echo_command(clifd,command,argument,buf,judge);
}

void echo_command(int clifd,char *command,char *argument,char *buf,int judge)
{
    if((0==strcmp(argument+2,".."))||(0==strcmp(argument+2,".")))
        {
            infofunc("arugement ban . and ..");
            return;
        }
	if(0==strcmp(command,"GET"))
	{
		Get(clifd,argument,judge);
	}
	else if(0==strcmp(command,"OPTIONS"))
	{
		Options(clifd,judge);
	}
	else if(0==strcmp(command,"POST"))
	{
		Post(clifd,argument,buf,judge);
	}	
	else if(0==strcmp(command,"head"))
	{
		Head(clifd,argument,judge);
	}
	else if(0==strcmp(command,"ERROR"))
	{
		infofunc("command error!");
		return;
	}
}

void Get(int clifd,char *argument,int judge)
{
    int lengths;
	struct stat info;
	struct dirent *dirents;
	DIR *dir;
	char buf[MAXBUF];
	bzero(buf,MAXBUF);
	int ret;
	if(stat(argument,&info))
	{
		if((errno==ENOENT)||(errno==ENOTDIR))
           senderror(clifd,2,judge);
		else
        {
        sprintf(buf,"HTTP/1.1 200 OK\r\nServer:ywang\r\nContent-Type:text/html;charset=UTF-8\r\n\r\n<html><head><title>%d - %s</title></head>"
			"<body><font size=+4>Ywang's server</font><br><hr width=\"100%%\"><br><center>"
			"<table border cols=3 width=\"100%%\">",errno,strerror(errno));	
        lengths=strlen(buf);    
		ret=sendall(clifd,buf,&lengths,judge);
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);	
		sprintf(buf,"</table><font color=\"CC0000\" size=+2>Please contact the administrator consulting why appear as follows error message：\n%s %s</font></body></html>",argument+2,strerror(errno));
		lengths=strlen(buf);
        ret=sendall(clifd,buf,&lengths,judge);
		if(ret==-1)
			{
				infofunc("sendall error");	
			}
        }   
	}
    else if(S_ISREG(info.st_mode))
	{
       char *type=file_type(argument);
       sendmsgs(clifd,type,argument,info.st_size,judge);
    }
    else if(S_ISDIR(info.st_mode))
    {
        dir=opendir(argument);
        char buf[MAXBUF];
        bzero(buf,MAXBUF);
        sprintf(buf,"HTTP/1.1 200 OK\r\n""Server:ywang\r\nContent-Type:text/html; charset=UTF-8\r\n\r\n<html><head><title>%s</title></head>"  
             "<body><font size=+4>Linux directory access server</font><br><hr width=\"100%%\"><br><center>"  
             "<table border cols=3 width=\"100%%\">", argument+2); 
        lengths=strlen(buf);
        ret=sendall(clifd,buf,&lengths,judge);
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);	
		sprintf(buf,"<caption><font size=+3>dir %s</font></caption>\n",argument+2);
        lengths=strlen(buf);
        ret=sendall(clifd,buf,&lengths,judge);
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);
		sprintf(buf,"<tr><td>name</td><td>大小</td><td>change time</td></tr>\n");
        lengths=strlen(buf);
        ret=sendall(clifd,buf,&lengths,judge);
		if(ret==-1)
			{
				infofunc("sendall error!");
                return ;
            }
		bzero(buf,MAXBUF);
		if(NULL==dir)
		{
			sprintf(buf,"</table><font color=\"CC0000\" size=+2>%s</font></body></html>", strerror(errno));
			lengths=strlen(buf);
            ret=sendall(clifd,buf,&lengths,judge);
			if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
			return ;
		}
	    while((dirents=readdir(dir))!=NULL)
        {
           if((0==strcmp(dirents->d_name,","))||(0==strcmp(dirents->d_name,",,")))
            continue;
           strcpy(buf,dirents->d_name);
           lengths=strlen(buf);
           ret=sendall(clifd,buf,&lengths,judge);
 		    if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
            }       
           bzero(buf,MAXBUF); 
        }
        sprintf(buf,"</table></body></html>");
           lengths=strlen(buf);
           ret=sendall(clifd,buf,&lengths,judge);
 		    if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
            }       
           bzero(buf,MAXBUF);
        closedir(dir);
	}
    else 
    {
        sprintf(buf, "HTTP/1.1 200 OK\r\nServer:ywang\r\nContent-Type:text/html;charset=UTF-8\r\n\r\n"
        	"<html><head><title>permission denied</title></head>"  
            "<body><font size=+4>Linux directory access server</font><br><hr width=\"100%%\"><br><center>"  
             "<table border cols=3 width=\"100%%\">");
        lengths=strlen(buf);
        ret=sendall(clifd,buf,&lengths,judge);
 		    if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
            }       
        bzero(buf,MAXBUF);
        sprintf(buf,"</table><font color=\"CC0000\" size=+2>You visit resources '%s' be under an embargo，Please contact the administrator to solve!</font></body></html& gt;", argument+2);
        lengths=strlen(buf);
        ret=sendall(clifd,buf,&lengths,judge);
 		    if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
            }       
        bzero(buf,MAXBUF);
	}
}

char *file_type(char *argument)
{
    char *temp;
    if(NULL!=(temp=strchr(argument,'.')))
        return temp+1;
    return NULL;
}

void sendmsgs(int clifd,char *type,char *argument,int length,int judge)
{
    int lengths;
    int err_bit=0;
    int fd;
    int nbytes,n;
    char buf[MAXBUF];
    if(0>(fd=open(argument,O_RDONLY))) //open maybe dangerous because work_dir's question   
        err_bit=1;
    sendhead(clifd,type,length,err_bit,judge);
    bzero(buf,MAXBUF);
    while((nbytes=recv(fd,buf,MAXBUF,0))>0) 
    {
        lengths=strlen(buf);
        n=sendall(clifd,buf,&lengths,judge);
        bzero(buf,MAXBUF);
        if(n==-1)
        break;
    }
    if(nbytes==-1||n==-1)
       { 
           close(fd);
           senderror(clifd,1,judge); 
       }
    if(nbytes==0)
        close(fd);
}

void sendhead(int clifd,char *type,int length,int err_bit,int judge)
{
   int lengths;
   char *content_type=NULL;
   if(0==strcmp(type,"html"))
     content_type="text/html";
   else if(0==strcmp(type,"gif"))
     content_type="image/gif";
   else if(0==strcmp(type,"jpg"))
    content_type="image/jpg";
   else
    {
        content_type=NULL;
        err_bit=1;
    }    
    char buf[MAXBUF];
    bzero(buf,MAXBUF);
    sprintf(buf,"HTTP/1.1 %s\r\nContent-type: %s\r\nContent-length: %d\r\n\r\n",content_type,req[err_bit],length);
    lengths=strlen(buf); 
    if(-1==sendall(clifd,buf,&lengths,judge))
        infofunc("send head error!");
}

void senderror(int clifd,int bit,int judge)//send 404 or 500 error to client
{
    int lengths;
    char buf[MAXBUF];    
    bzero(buf,MAXBUF);
    sprintf(buf,"HTTP/1.1 %s\r\n\r\nReason: The file/dir you requested is not exist!",req[bit]);
    lengths=strlen(buf);
    if(-1==sendall(clifd,buf,&lengths,judge))
        infofunc("senderror error!");
}

void Options(int clifd,int judge)
{
	int lengths;
    char buf[MAXBUF];
	bzero(buf,MAXBUF);
	sprintf(buf,"HTTP/1.1 200 OK\r\nAllow: GET, POST, HEAD, OPTIONS\r\nContent-length: 0\r\n\r\n");
	int ret=sendall(clifd,buf,&lengths,judge);
 	if(ret==-1)
		senderror(clifd,1,judge);    
}

void Post(int clifd,char *argument,char *buf,int judge) //cgi
{
	int num1;
    pid_t pid;
    char ptr1[10];
    char ptr2[10];
    bzero(ptr1,10);
    bzero(ptr2,10);
    if(-1==sscanf(buf,"\r\n\r\nnum=%d",&num1))
        {
            infofunc("Post num error!");
            return;
        }
    sprintf(ptr1,"%d",clifd);
    sprintf(ptr2,"%d",num1);
    pid=fork();
    if(0>pid)
        {
            infofunc("post fork error!");
            return;
        }
    if(pid==0)
        execl("./cgi-bin/cal.cgi",ptr1,ptr2,(char *)0);
    return;   
}

void Head(int clifd,char *argument,int judge)
{	
    int err_bit;
	struct stat info;
	char *type=file_type(argument);
	if(stat(argument,&info))
		err_bit=2;
	else
		err_bit=0;
	
	sendhead(clifd,type,info.st_size,err_bit,judge);
}

void ssl_init(SSL_CTX *ctx)  //https
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
    ctx=SSL_CTX_new(SSLv23_server_method());    
    SSL_CTX_new(SSLv23_server_method());
	if(ctx==NULL)
	{
		errorfunc("ssl_init error!");
	}
    if(SSL_CTK_use_PrivateKey_file(ctx,,SSL_FILETYPE_PEM)<=0)
    {
        errorfunc("ssl_init_private_key error!");
    }
    if(!SSL_CTX_check_private_key(ctx)){
        errorfunc("ssl_check_private error!");
    }
}

int make_socket_ssl(int sockfds)
{
    sockfds=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in serv_addr;
    if(-1==sockfd)
    {
        errorfunc("sockfds socket error!");
    }   
    bzero(&serv_addr,sizeof(sockaddr_in));  
    if(NULL==ip)
    char *ip_temp=get_defaultip();
    else 
    char *ip_temp=ip;
    int port_temp=443;
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(port_temp);
    serv_addr.sin_addr.s_addr=inet_addr(ip);
    if(-1==bind(sockfds,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr)))
    {
        errorfunc("bind ssl error!");
    }
    if(-1==listen(sockfds,DEFAULTBACKLOG))
    {
        errorfunc("listen ssl error!");
    }
    return sockfds;
}

int tpool_create(int thr_num)
{
    int i;
    tpool=(tpool_t *)calloc(1,sizeof(tpool_t));
    if(!tpool)    
        errorfunc("creare tpool error!");
    
    tpool->max_thr_num=thr_num;
    tpool->shutdown=0;
    tpool->queue_head=NULL;
    if(pthread_mutex_init(&tpool->queue_lock,NULL)!=0)
        errorfunc("mutex init error!");
    if(pthread_cond_init(&tpool->queue_ready,NULL)!=0)
        errorfunc("cond init error!");
    
    tpool->thr_id=(pthread_t *)calloc(thr_num,sizeof(pthread_t));
    if(!tpool->thr_id)
        errorfunc("calloc error!");
    for(i=0;i<thr_num;i++)
    {
        if(pthread_create(&tpool->thr_id[i],NULL,thread_routine,NULL)!=0)
            errorfunc("pthread create error!");
    }

    return 0;
}

void tpool_destroy()
{
    int i;
    tpool_work_t *member;
    
    if(tpool->shutdown)
       return;
    tpool->shutdown=1;
    
    pthread_mutex_lock(&tpool->queue_lock);
    pthread_cond_broadcast(&tpool->queue_ready);
    pthread_mutex_unlock(&tpool->queue_lock);
    
    for(i=0;i<tpool->max_thr_num;i++)
    {
        pthread_join(tpool->thr_id[i],NULL);       
    }
    free(tpool->thr_id);

    while(tpool->queue_head){
        member=tpool->queue_head;
        tpool->queue_head=tpool->queue_head->next;
        free(member);
    }

    pthread_mutex_destroy(&tpool->queue_lock);
    pthread_cond_destroy(&tpool->queue_ready);
    
    free(tpool);
}

int tpool_add_work(void *(*routine)(void *),void *arg)
{
    tpool_work_t *work,*member;           
    if(!routine)
    {  
        infofunc("routine error!");
        return -1;
    }
    work=malloc(sizeof(tpool_work_t));
    if(!work)
    {
        infofunc("work error!"); 
        return -1;
    }
    work->routine=routine;
    work->arg=arg;
    work->next=NULL;
    
    pthread_mutex_lock(&tpool->queue_lock);
    member=tpool->queue_head;
    if(!member)
    {
        tpool->queue_head=work;
    }
    else 
    {
        while(!tpool->queue_head->next)
            tpool->queue_head=tpool->queue_head->next;
        tpool->queue_head->next=work;
    }
    pthread_cond_signal(&tpool->queue_ready);
    pthread_mutex_unlock(&tpool->queue_lock);
    
    return 0;   

}

void *func1(void *arg)
{
    sign *var=(sign *)arg;		    
    int fd=var->clifd;
    int judge=var->type;
    getinfomation(fd,judge);
    free(arg);
}

void *func2(void *arg)
{
    sign2 *var=(sign2 *)arg;
    int listenfd=var->listenfd;
    int epollfd=var->epollfd;
    char buf[MAXBUF];
	bzero(buf,MAXBUF);
	struct sockaddr_in cli_address;
	socklen_t cli_addrlength=sizeof(struct sockaddr_in);	
	int clifd=accept(listenfd,(struct sockaddr *)&cli_address,&cli_addrlength);
	if(clifd<0)
	{	
		infofunc("clifd accept error!");
		goto end;
	}
	sprintf(buf,"connect %s:%d!",inet_ntoa(cli_address.sin_addr),ntohs(cli_address.sin_port));		
	addfd(epollfd,clifd);
	infofunc(buf);
    end:;
}

void *func3(void *arg)
{
        //ssl handshake





}
#endif HTTP_SERVER_H__
