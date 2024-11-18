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



#define KEY_VALUE   0X01
#define INVAKEY     0X00

int main(int argc,char *argv[])
{
    int fd,ret;
    char *filename;
    unsigned char key_value;
    if(argc != 2){
        printf("Error Usage!!!\r\n");
        return -1;
    }
    filename = argv[1];
    fd = open(filename ,O_RDWR | O_NONBLOCK);
    if(fd < 0){
        printf("file %s open failed!\r\n",filename);
        return -1;
    }

#if 0 /* select 函数 */
    fd_set readfds;
    struct timeval timeout;
    while(1){
        FD_ZERO(&readfds);
        FD_SET(fd,&readfds);        
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        ret = select(fd+1,&readfds,NULL,NULL,&timeout);
        switch (ret) {
            case 0: 
                printf("time_out!!!\r\n");
                break;
            case -1:
                /*ERROR*/
                break;
            default:
                if(FD_ISSET(fd,&readfds)){
                    ret = read(fd,&key_value,sizeof(key_value));
                    if(ret < 0){  /* 错误或者按键没按下 */
                    
                    }else{
                        if(key_value){
                            printf("KEY PRESS, value = %x\r\n",key_value);
                        }
                    }
                }
                break;
        }
    }
#endif

#if 0  /*  poll 函数  */
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;
    while(1){
        ret = poll(&fds,1,500);
        if(ret){
            ret = read(fd,&key_value,sizeof(key_value));
            if(ret < 0){  /* 错误或者按键没按下 */

            }else{
                if(key_value){
                    printf("KEY PRESS, value = %x\r\n",key_value);
                }
            }
        }
        else if(ret == 0){
            printf("time_out!!!\r\n");
        }
        else{
            /* 错误 */
        }
    }
#endif

#if 1  /*  epoll 函数  */
    int epoll;
    epoll = epoll_create(1);
    struct epoll_event epoll_event,reepoll_event;
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = fd;
    epoll_ctl(epoll,EPOLL_CTL_ADD,fd,&epoll_event);
    while(1){
        ret = epoll_wait(epoll,&reepoll_event,1,500);
        if(ret > 0 && reepoll_event.data.fd == fd){
            ret = read(fd,&key_value,sizeof(key_value));
            if(ret < 0){  /* 错误或者按键没按下 */

            }else{
                if(key_value){
                    printf("KEY PRESS, value = %x\r\n",key_value);
                }
            }
        }
        else if(ret == 0){
            printf("time_out!!!\r\n");
        }
        else{
            /* 错误 */
        }
    }
#endif

    ret = close(fd);
    if(ret < 0){
        printf("file %s close failed! \r\n",filename);
        return -1;
    }
    return 0; 
}