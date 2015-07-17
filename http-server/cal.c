#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc,char **argv)
{
    int num;
    num=atoi(argv[1]);
    int clifd;
    clifd=atoi(argv[0]);
    num*=10;
    char buf[100];
    bzero(buf,100);
    sprintf(buf,"The result is %d",num);
    int length;
    length=strlen(buf);

    close(STDOUT_FILENO);
    dup2(clifd,1);
    printf("HTTP/1.1 200 OK\r\nContent-type: text/plain\r\nContent-length: %d\r\n\r\n%s",length,buf);
    exit(0);    
}
