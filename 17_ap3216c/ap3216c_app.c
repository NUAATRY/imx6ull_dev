#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"
#include <stdlib.h>
#include <sys/ioctl.h>



int main(int argc,char *argv[])
{
    int fd,ret;
    char *filename;
    unsigned short databuf[3];
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
        ret = read(fd,databuf,sizeof(databuf));
        if(ret == 0){
            printf("ir = %d, als = %d, ps = %d\r\n",databuf[0],databuf[1],databuf[2]);
        }else if(ret < 0){
            break;
        }
        usleep(200000);
    }
    ret = close(fd);
    if(ret < 0){
        printf("file %s close failed! \r\n",filename);
        return -1;
    }
    return 0;
}