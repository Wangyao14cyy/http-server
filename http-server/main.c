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
#include "http.h"

char buf[MAXBUF+1];
char *ip=NULL;
char *_log=NULL;  //-
char *dirroot=NULL;
int port=0;
int daemon_check=0; // -D option
int logfd=0;  
int tls_check=0;
char *errno_q=NULL;
char *req[3]={"200 OK","500 Internal Server Error","404 Not Found"};

SSL_CTX *ctx; 

extern int errno;

int main(int argc,char **argv)
{
	printf("Http server welcome you!\n");
	
	if(1!=argc)
	getoption(argc,argv);//It's hard to learn how to use it 	
	
	if(NULL==_log)
		logfd=open(DEFAULTLOG,O_WRONLY | O_APPEND | O_CREAT);
	else
		logfd=open(_log,O_WRONLY | O_CREAT | O_APPEND);
	
	daytime();
	int sockfd;
	if(daemon_check)
		daemons();
	if(tls_check)
		ssl_init();
	signal(SIGPIPE,SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	sockfd=make_socket(sockfd);
	if(sockfd<0)
		errorfunc("sockfd error!");
	int addrlen = 1;  
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&addrlen,sizeof(addrlen));//set the port quickly reuse
	if(tls_check)
		sockfd=openssl(sockfd);
	struct epoll_events events[MAXEVENTS];//question
	int epollfd=epoll_create[MAXEVENTS];
	addfd(epollfd,sockfd);
	chdir("/home/wangyao/web");
	while(1)
	{
		int ret=epoll_wait(epollfd,events,MAXEVENTS,-1);//epoll func should be use in here
		if(ret<0)
			errorfunc("epoll_wait error!");
		lt(events,ret,epollfd,sockfd);
	}
	close(sockfd);
	exit(0);
}
