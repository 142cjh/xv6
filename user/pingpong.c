#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//创建两个管道，一个父读子写，一个父写自读
int main(int argc, char *argv[])
{
    int p2c[2],c2p[2];
    if(pipe(p2c)<0)
    {
        printf("pipe error\n");
        exit(-1);
    }
    if(pipe(c2p)<0)
    {
        printf("pipe error\n");
        exit(-1);
    }
    int pid=fork();
    if(pid==0)
    {
        //子进程先读后写
        char buf[10];
        read(p2c[0],buf,10);
        printf("%d:received ping\n",getpid());
        write(c2p[1],"o",2);
    }
    else if(pid>0)
    {
        //父进程先写后读
        write(p2c[1],"p",2);
        char buf[10];
        read(c2p[0],buf,10);
        printf("%d:received pong\n",getpid());
    }

    close(p2c[0]);
    close(p2c[1]);
    close(c2p[0]);
    close(c2p[1]);
    exit(0);
}