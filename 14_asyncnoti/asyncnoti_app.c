#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"
#include "poll.h"
#include "sys/select.h"
#include "sys/time.h"
#include "linux/ioctl.h"
#include <sys/epoll.h>
#include "signal.h"


#define KEY_VALUE   0X01
#define INVAKEY     0X00

static int fd = 0;
static unsigned int key_value;

#if 1
static void sigio_signal_func(int signum)
{
    int ret = 0;

    ret = read(fd,&key_value,sizeof(key_value));
    if(ret < 0){  /* 错误或者按键没按下 */

    }else{
        if(key_value){
            printf("KEY PRESS, value = %x\r\n",key_value);
        }
    }
}
#endif

int main(int argc,char *argv[])
{
    int ret;
    int flags = 0;
    char *filename;
    if(argc != 2){
        printf("Error Usage!!!\r\n");
        return -1;
    }
    filename = argv[1];
    fd = open(filename ,O_RDWR);
    if(fd < 0){
        printf("file %s open failed!\r\n",filename);
        return -1;
    }
#if 1
    signal(SIGIO,sigio_signal_func);
    fcntl(fd,__F_SETOWN,getpid());      /* 将当前进程的进程号告诉给内核 */
    flags = fcntl(fd,F_GETFD);          /* 获取当前的进程状态 */
    fcntl(fd,F_SETFL,flags | O_ASYNC ); /* 设置进程启用异步通知功能 */
#endif
    while(1){
        sleep(1);
#if 0
        ret = read(fd,&key_value,sizeof(key_value));
        if(ret < 0){  /* 错误或者按键没按下 */
        }
        else{
            if(key_value)
            {
                printf("KEY PRESS, value = %x\r\n",key_value);
            }
        }
#endif
    }

    ret = close(fd);
    if(ret < 0){
        printf("file %s close failed! \r\n",filename);
        return -1;
    }
    return 0; 
}