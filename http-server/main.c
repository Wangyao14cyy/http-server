#include "http.h"
extern int errno;

int main(int argc,char **argv)
{
    extern pthread_mutex_t mutex_all;
	printf("Http server welcome you!\n");
    tpool_create(10);	
	if(1!=argc)
	getoption(argc,argv);//It's hard to learn how to use it  
	if(NULL==_log)
		logfd=open(DEFAULTLOG,O_WRONLY | O_APPEND | O_CREAT);
	else
		logfd=open(_log,O_WRONLY | O_CREAT | O_APPEND);
	
	daytime();
	int sockfd,sockfds;
	if(daemon_check)
		daemons();
	signal(SIGPIPE,SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	sockfd=make_socket(sockfd);
    sockfds=make_socket_ssl(sockfds);
	if(sockfd<0||sockfds<0)
		errorfunc("sockfd error!");
	int addrlen = 1;  
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&addrlen,sizeof(addrlen));//set the port quickly reuse
    setsockopt(sockfds,SOL_SOCKET,SO_REUSEADDR,&addrlen,sizeof(addrlen));
    struct epoll_event events[MAXEVENTS];//question
	int epollfd=epoll_create(MAXEVENTS);
	addfd(epollfd,sockfd,0);
    addfd(epollfd,sockfds,0);
	chdir("/home/wangyao/web");
	while(1)
	{
		int ret=epoll_wait(epollfd,events,MAXEVENTS,-1);//epoll func should be use in here
		if(ret<0)
			errorfunc("epoll_wait error!");
		lt(events,ret,epollfd,sockfd,sockfds);
	}
    close(sockfds);
	close(sockfd);
    close(epollfd);
    sleep(10);
    tpool_destroy();
    SSL_CTX_free(ctx);         
	exit(0);
}
