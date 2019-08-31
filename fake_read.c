/* pretends to be a portsdown */
#include <stdio.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
  
int main() { 
    int num;
    int ret;

    int fd_status_fifo; 

    char status_message[14];
  
    /* Open status FIFO for read only  */
    ret=mkfifo("longmynd_main_status", 0666);
    fd_status_fifo = open("longmynd_main_status", O_RDONLY); 
    if (fd_status_fifo<0) printf("Failed to open status fifo\n");

    printf("Listening\n"); 

    while (1) {
        num=read(fd_status_fifo, status_message, 1);
        status_message[num]='\0';
        if (num>0) printf("%s",status_message);
    } 
    close(fd_status_fifo); 
    return 0; 
}

