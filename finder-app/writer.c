#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<syslog.h>
#include <string.h>


int main(int argc, char *argv[]){

    int fd;
    unsigned long word = 1720;
    ssize_t nr;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <text>\n", argv[0]);
        return 1;
    }

    const char *fileName = argv[1];
    const char *text = argv[2];

    openlog("writer-log", LOG_PID, LOG_USER);

    syslog(LOG_INFO, "Start logging");
   
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