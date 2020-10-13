#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char *argv[])
{
    if(argc!=2)
    {
        fprintf(2,"usage:sleep pattern [file....]\n");
        exit(1);
    }
    //使用atoi函数进行字符串转换为整数
    sleep(atoi(argv[1]));
    exit(0);
}