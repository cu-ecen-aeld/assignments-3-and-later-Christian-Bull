#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<syslog.h>
#include <string.h>


int main(){

    int fd;
    char fileName[100];
    char text[256];
    unsigned long word = 1720;
    size_t count;
    ssize_t nr;

    openlog("writer-log", LOG_PID, LOG_USER);

    syslog(LOG_INFO, "Start logging");
    
    printf("Enter full path to filename: \n");
    scanf("%s", fileName);

    printf("Enter text: \n");
    scanf(" %[^\n]", text);

    fd = open (fileName, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
            perror ("open");
            syslog(LOG_ERR, "Opening file failed");
            return -1;
    }
    
    nr = write(fd, text, strlen(text));
    if (nr == -1) {
        perror("write");
        syslog(LOG_ERR, "Writing to file failed");
        close(fd);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %ld to %s", word, fileName);
    
    if (close (fd) == -1)
        perror ("close");
        syslog(LOG_ERR, "Closing file failed");

    closelog();
    return 0;
}