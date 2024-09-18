#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"


static char usrdata[] = {"usr data!"};

/*
 argc:应用程序参数个数
 argv[]：具体的参数内容，字符串形式
*/
int main(int argc,char *argv[])
{
    int ret = 0;
    int fd = 0;
    char *filename;
    char readbuf[100],writebuf[100];

    if(argc != 3){
        printf("error usage\r\n");
        return -1;
    }
    filename = argv[1];

    fd = open(filename, O_RDWR);
    if(fd < 0){
        printf("can`t open file %s\r\n",filename);
        return -1;
    }

    if(atoi(argv[2]) == 1){//等于1为读
        ret =  read(fd, readbuf, 50);
        if(ret < 0){
            printf("can`t read file %s\r\n",filename);
            return -1;
        }
        else{
            printf("read data:%s\r\n",readbuf);
        }
    }
    if(atoi(argv[2]) == 2){

        memcpy(writebuf,usrdata,sizeof(usrdata));
        ret = write(fd,writebuf,50);
        if(ret < 0){
            printf("can`t write file %s\r\n",filename);
            return -1;
        }
    }
    ret = close(fd); 
    if(ret < 0){
        printf("cna`t close file %s \r\n",filename);
        return -1;
    }
    return 0;
}
