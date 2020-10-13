#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/param.h"


int main(int argc,char *argv)
{
    //父进程写-》子进程读
    //子进程写-》父进程读
    //创建两个管道，父进程处理：先写后读，子进程处理：先读后写
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);

    if(argc!=1)
    {
        printf("error");
        exit(1);
    }
    
    int pid=fork();
    
    //父进程处理
    if(pid>0)
    {
        char buf[10];
        close(p1[0]);
        close(p2[1]);
        int n=write(p1[1],"1",1);
        if(n==-1)
        {
            printf("%d write error",getpid());
        }
        int m=read(p2[0],buf,1);
        if(m==-1)
        {
            printf("%d read error",getpid());
        }
        printf("%d:received pong \n",getpid());
        wait(0);
    }
    //子进程处理
    else
    {
        char buf[10];
        close(p1[1]);
        close(p2[0]);
        int m=read(p1[0],buf,1);
        if(m==-1)
        {
            printf("%d read error",getpid());
        }
        printf("%d:received ping \n",getpid());
        int n=write(p2[1],buf,1);
        if(n==-1)
        {
            printf("%d write error",getpid());
        }
    }
    exit(0);
}