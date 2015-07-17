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

#define DEFAULTPORT 80
#define DEFAULTLOG "/home/wangyao/log/http.log"
#define DEFAULTBACKLOG 10 //why 10?
#define DEFAULTDIR "/home/wangyao/http"

#define HOSTLEN 256
#define MAXBUF 1024
#define MAXEVENTS 5000
#define MAXPATH 256

void derrormsg(char *msg);  //send the error message in the backgroud
void errormsg(char *msg);   //send the error message not in the backgroud
void dinfomsg(char *msg);	  //send the proper message in the backgroud 	
void infomsg(char *msg);    //send the proper message not in the backgroud

int make_socket(int sockfd);          //The sockfd is made to socket,bind,listen and return 
char *get_defaultip();				  //If you didn't choose a ip-address,it's will get the default ip-address 

int sendall(int fd,char *buf,int *len);        //remember turn to next line by "\n"   
void daemons(void);
void file_not_found(int clifd,char *args);
void daytime(void);//report time when you start the server!!!!
int openssl(int sockfd);
void errorfunc(char *msg);
void infofunc(char *msg);
void ssl_init(void );
int setnonblock(int fd);
void addfd(int epollfd,int fd);
void lt(struct epoll_events *events,int number,int epollfd,int listenfd);//switch to LT mode
void getinfomation(int clifd);
void echo_command(int clifd,char *command,char *argument,char *buf);
void Get(int clifd,char *argument);
void Head(int clifd,char *argument);
void Options(int clifd);
void Post(int clifd,char *argument,char *buf); 
char *file_type(char *argument);
void sendmsgs(int clifd,char *type,char *argument,int length);
void sendhead(int clifd,char *type,int length,int error_bit);
void senderror(int clifd,int bit);

SSL_CTX *ctx; 

void daemons(void)
{		
    umask(0);
	if(0!=fork())
		exit(0);
	
	if(-1==setsid())
		derrormsg();
	
	if(0!=fork())
		exit(0);
	
	chdir("/");		
	
	close(STDOUT_FILENO);
	close(STDIN_FILENO);
	close(STDERR_FILENO);
	
	int fd=dup("/dev/null");
	dup2(fd,STDOUT_FILENO);
	dup2(fd,STDERR_NO);
}

void daytime(void)
{
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
	
	if(-1==connect(sockfd,(struct sockaddr *)&cli_address,sizeof(sockaddr_in)))
		errormsg("when daytime connect error!");
	
	while((n=read(sockfd,recvnum,MAXBUF))>0)
		{
			sendall(logfd,recvnum,strlen(recvnum));		
			sendall(STDOUT_FILENO,recvnum,strlen(recvnum);
		}	
	if(n<0)
		errormsg("when daytime output error!");
	close(sockfd);
}

int sendall(int fd,char *buf,int *len) 
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
	errno_q=strerror(errno);
	sendall(logfd,errno_q,&strlen(errno_q));
	sendall(logfd,msg,&strlen(msg));
	abort();
}

void errormsg(char *msg)
{
	errno_q=strerror(errno);	
	sendall(logfd,errno_q,&strlen(errno_q));
	sendall(logfd,msg,&strlen(msg));
	sendall(STDOUT_FILENO,errno_q,&strlen(errno_q));
	sendall(STDOUT_FILENO,msg,&strlen(msg));
	abort();
}

void infomsg(char *msg)
{
	sendall(logfd,msg,&strlen(msg));
	sendall(STDOUT_FILENO,msg,&strlen(msg));
}

void dinfomsg(char *msg)
{
	sendall(logfd,msg,&strlen(msg));
}

int make_socket(int sockfd)
{
	struct sockaddr_in servaddr;
	bzero(&servaddr,sizeof(struct sockaddr_in));
	if((sockfd=socket(AF_INET,SOCK_STREAM,0)<0)
		errorfunc("socket error!");	
	if(0==port)
		port=DEFAULTPORT;
	if(NULL==ip)
	ip=get_defaultip();
	
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(port);
	
	if(1!=inet_pton(AF_INET,ip,(void *)&servaddr))
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
		{"tls",0,NULL,'T'},
		{0,0,0,0},
		};
	char *l_opt_arg=NULL;
	char *const short_options="I:L:P:D:BT";	
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
			case 'T':
				tls_check=1;
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
	if(-1==epollctl(epollfd,EPOLL_CTL_ADD,fd,&event))
		errorfunc("epollctl error!");
	setnonblock(fd);
}

void lt(struct epoll_events *events,int number,int epollfd,int listenfd)
{
	char buf[MAXBUF];
	for(int i=0;i<number;i++)
	{
		bzero(buf,MAXBUF);
		int sockfd=events[i].data.fd;
		if(sockfd==listenfd)
		{
			struct sockaddr_in cli_address;
			socklen_t cli_addrlength=sizeof(struct sockaddr_in);
			int clifd=accept(listenfd,(struct sockaddr *)&cli_address,&cli_addrlength);
			if(clifd<0)
				{	
					infofunc("clifd accept error!");
					continue;
				}
			sprintf(buf,"connect %s:%d!",inet_ntoa(cli_address.sin_addr.s_addr),ntohs(cli_address.sin_port));		
			addfd(epollfd,clifd);
			infofunc(buf);
		}
		else if(events[i].events & EPOLLIN)
			getinfomation(sockfd);
		else 
			close(sockfd);
	}
}

void getinfomation(int clifd)
{
	char buf[MAXBUF];
	int recvnum;
	char command[10];
	char argument[MAXBUF];
	char mes[MAXBUF];
	bzero(buf,MAXBUF);
	bzero(argument,MAXBUF];
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
		command="ERROR";

	echo_command(clifd,command,argument,buf);

}

void echo_command(int clifd,char *command,char *argument,char *buf)
{
    if((0==strcmp(argument+2,".."))||(0==strcmp(argument+2,".")))
        {
            infofunc("arugement ban . and ..");
            return;
        }
	if(0==strcmp(command,"GET"))
	{
		Get(clifd,argument);
	}
	else if(0==strcmp(command,"OPTIONS"))
	{
		Options(clifd);
	}
	else if(0==strcmp(command,"POST"))
	{
		Post(clifd,argument,buf);
	}	
	else if(0==strcmp(command,"head"))
	{
		Head(clifd,argument)
	}
	else if(0==strcmp(command,"ERROR"))
	{
		infofunc("command error!");
		return;
	}
}

void Get(int clifd,char *argument)
{
	struct stat info;
	struct dirent *dirents;
	DIR *dir;
	char buf[MAXBUF];
	bzero(buf,MAXBUF);
	int ret;
	if(stat(argument,&info))
	{
		if((errno==ENOENT)||(errno==ENOTDIR))
           senderror(clifd,2);
		else if
        {
        sprintf(buf,"HTTP/1.1 200 OK\r\nServer:ywang\r\nContent-Type:text/html;charset=UTF-8\r\n\r\n<html><head><title>%d - %s</title></head>"
			"<body><font size=+4>Ywang's server</font><br><hr width=\"100%%\"><br><center>"
			"<table border cols=3 width=\"100%%\">",errno,strerror(errno));			
		ret=sendall(clifd,buf,&strlen(buf));
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);	
		sprintf(buf,"</table><font color=\"CC0000\" size=+2>Please contact the administrator consulting why appear as follows error message：\n%s %s</font></body></html>",argument+2,strerror(errno));
		ret=sendall(clifd,buf,&strlen(buf));
		if(ret==-1)
			{
				infofunc("sendall error");	
			}
        }   
	}
    else if(S_ISREG(info.st_mode))
	{
       char *type=file_type(argument);
       sendmsgs(clifd,type,argument,info.st_size);
    }
    else if(S_ISDIR(info.st_mode))
    {
        dir=opendir(argument);
        char buf[MAXBUF];
        bzero(buf,MAXBUF);
        sprintf(buf,"HTTP/1.1 200 OK\r\n""Server:ywang\r\nContent-Type:text/html; charset=UTF-8\r\n\r\n<html><head><title>%s</title></head>"  
             "<body><font size=+4>Linux directory access server</font><br><hr width=\"100%%\"><br><center>"  
             "<table border cols=3 width=\"100%%\">", argument+2);  
        ret=sendall(clifd,buf,&strlen(buf));
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);	
		sprintf(buf,"<caption><font size=+3>dir %s</font></caption>\n",argument+2);
        ret=sendall(clifd,buf,&strlen(buf));
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);
		sprintf(buf,"<tr><td>name</td><td>大小</td><td>change time</td></tr>\n");				
        ret=sendall(clifd,buf,&strlen(buf));
		if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
			}
		bzero(buf,MAXBUF);
		if(0==dir)
		{
			sprintf(buf,"</table><font color=\"CC0000\" size=+2>%s</font></body></html>", strerror(errno));
			ret=sendall(clifd,buf,&strlen(buf));
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
           buf=dirents->d_name;
           ret=sendall(clifd,buf,&strlen(buf));
 		    if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
            }       
           bzero(buf,MAXBUF); 
        }
        sprintf(buf,"</table></body></html>");
           ret=sendall(clifd,buf,&strlen(buf));
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
        ret=sendall(clifd,buf,&strlen(buf));
 		    if(ret==-1)
			{
				infofunc("sendall error!");
				return ;
            }       
        bzero(buf,MAXBUF);
        sprintf(buf,"</table><font color=\"CC0000\" size=+2>You visit resources '%s' be under an embargo，Please contact the administrator to solve!</font></body></html& gt;", argumet+2);
        ret=sendall(clifd,buf,&strlen(buf));
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
    if(NULL!=(temp=strchr(arg,'.')))
        return temp+1;
    return NULL;
}

void sendmsgs(int clifd,char *type,char *argument,int length)
{
    int err_bit=0;
    int fd;
    int nbytes,n;
    char buf[MAXBUF];
    if(0>(fd=open(argument,O_RDONLY))) //open maybe dangerous because work_dir's question   
        err_bit=1;
    sendhead(clifd,type,length,error_bit);
    bzero(buf,MAXBUF);
    while((nbytes=recv(fd,buf,MAXBUF,0))>0) 
    {
        n=sendall(clifd,buf,&strlen(buf));
        bzero(buf,MAXBUF);
        if(n==-1)
        break;
    }
    if(nbytes==-1||n==-1)
       { 
           close(fd);
           senderror(clifd,1); 
       }
    if(nbytes==0)
        close(fd);
}

void sendhead(int clifd,char *type,int length,int err_bit)
{
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
    sprintf(buf,"HTTP/1.1 %s\r\nContent-type: %s\r\nContent-length: %d\r\n\r\n",content_type,req[error_bit],length);
    
    if(-1==sendall(clifd,buf,&strlen(buf)))
        infofunc("send head error!");
}

void senderror(int clifd,int bit)//send 404 or 500 error to client
{
    char buf[MAXBUF];    
    bzero(buf,MAXBUF);
    sprintf(buf,"HTTP/1.1 %s\r\n\r\nReason: The file/dir you requested is not exist!",req[bit]);
    if(-1==sendall(clifd,buf,&strlen(buf)))
        infofunc("senderror error!");
}

void Options(int clifd)
{
	char buf[MAXBUF];
	bzero(buf,MAXBUF);
	sprintf(buf,"HTTP/1.1 200 OK\r\nAllow: GET, POST, HEAD, OPTIONS\r\nContent-length: 0\r\n\r\n");
	ret=sendall(clifd,buf,&strlen(buf));
 	if(ret==-1)
		senderror(clifd,1);    
}

void Post(int clifd,char *argument,char *buf) //cgi
{
	int num1;
    pid_t pid;
    char ptr1[10];
    char ptr2[10];
    bzero(ptr,10);
    bzero(ptr,10);
    if(-1==sscanf(buf,"\r\n\r\nnum=%d",num1))
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

void Head(int clifd,char *argument)
{	
	struct stat info;
	char *type=file_type(argument);
	if(stat(argument,&info))
		err_bit=2;
	else
		err_bit=0;
	
	sendhead(clifd,type,info.st_size,err_bit);
}

void ssl_init(void)  //https
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(SSLv23_server_method());
	if(ctx==NULL)
	{
		errorfunc("ssl_init error!");
	}
}

