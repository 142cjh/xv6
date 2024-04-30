/*思想：最后让每一个素数各一个管道存储
        循环第一次，2，得到所有素数，将2留下在左管道，其他素数加入右管道
        循环第二次，3，得到剩下素数，将3留下在左管道，其他素数加入右管道
        依次类推*/
#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1

const uint INT_LEN=sizeof(int);

/*读取左管道的第一个数据，等下单独存在左管道*/
int lpipe_first_data(int lpipe[2],int *dst)
{
    if(read(lpipe[RD],dst,sizeof(int))==sizeof(int))
    {
        printf("prime %d\n",*dst);
        return 0;
    }
    return -1;
}

/*读取左管道的数据，将不能被当前数整除的写入右管道*/
void transmit_data(int lpipe[2],int rpipe[2],int first)
{
    int data;
    //从左管道读取数据
    while(read(lpipe[RD],&data,sizeof(int))==sizeof(int))
    {
        //将无法整除的数据传递入右管道
        if(data%first)
        {
            write(rpipe[WR],&data,sizeof(int));
        }
    }
    //关闭左管道的读，无需再使用，此时左管道只存储一个素数
    close(lpipe[RD]);
    //关闭右管道的写，留下读端，作为下一个循环要用的左管道
    close(rpipe[WR]);
}

/*寻找素数*/
void primes(int lpipe[2])
{
    close(lpipe[WR]);
    int first;
    if(lpipe_first_data(lpipe,&first)==0)
    {
        int p[2];
        //当前的管道
        pipe(p);
        //分离出素数，并将其他素数加入下一个管道
        transmit_data(lpipe,p,first);

        if(fork()==0)
        {
            primes(p);//递归调用，在新的进程中调用
        }
        else
        {
            close(p[RD]);//子进程读取完毕，父进程关闭读端
            wait(0);
        }
    }
    exit(0);
}

int main(int argc,char *argv[])
{
    int p[2];
    pipe(p);

    //写入数据
    for (int i = 2; i <= 35; ++i) 
        write(p[WR], &i, INT_LEN);

    if (fork() == 0) {
        primes(p);
    } else {
        close(p[WR]);
        close(p[RD]);
        wait(0);
    }

    exit(0);
}