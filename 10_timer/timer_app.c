#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"
#include <stdlib.h>
#include <sys/ioctl.h>


#define CLOSE_CMD  			_IO(0XEF, 0X1)   /* 关闭定时器 */
#define OPEN_CMD   			_IO(0XEF, 0X2)   /* 打开定时器 */
#define SETPERIOD_CMD  		_IO(0XEF, 0X3)   /* 设置定时器周期命令 */

int main(int argc,char *argv[])
{
    int fd,ret;
    char *filename;
    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];
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

    while(1){
        printf("Input CMD:");
        ret = scanf("%d",&cmd);
        if(ret != 1){  /* 参数输入错误 */
            fgets(str,100,stdin);   /* 防止卡死 */
        }
        if(cmd == 1)
            cmd = CLOSE_CMD;
        else if(cmd == 2)
            cmd = OPEN_CMD;
        else if(cmd == 3){
            cmd = SETPERIOD_CMD;
            printf("Input Timer Period:");
            ret = scanf("%d",&arg);
            if(ret != 1){
                fgets(str,100,stdin); 
            }
        }
        ioctl(fd,cmd,arg);
    }
    ret = close(fd);
    if(ret < 0){
        printf("file %s close failed! \r\n",filename);
        return -1;
    }
    return 0;
}