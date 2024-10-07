#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"

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
    fd = open(filename ,O_RDWR);
    if(fd < 0){
        printf("file %s open failed!\r\n",filename);
        return -1;
    }
    while(1){
        read(fd,&key_value,sizeof(key_value));
        if(key_value == KEY_VALUE){
            printf("KEY PRESS, value = %x\r\n",key_value);
        }
    }
    ret = close(fd);
    if(ret < 0){
        printf("file %s close failed! \r\n",filename);
        return -1;
    }
    return 0;
}