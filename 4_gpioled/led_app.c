#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"

#define LEDON 1
#define LEDOFF 0

int main(int argc,char *argv[])
{
    int fd,ret;
    char *filename;
    unsigned char databuf;
    if(argc != 3){
        printf("Error Usage!!!\r\n");
        return -1;
    }
    filename = argv[1];
    fd = open(filename ,O_RDWR);
    if(fd < 0){
        printf("file %s open failed!\r\n",filename);
        return -1;
    }
    databuf = atoi(argv[2]);
    ret = write(fd,&databuf,sizeof(databuf));
    if(ret < 0){
        printf("LED control failed\r\n");
        close(fd);
        return -1;
    }
    ret = close(fd);
    if(ret < 0){
        printf("file %s close failed! \r\n",filename);
        return -1;
    }
    return 0;
}