#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<syslog.h>


int fd;
char fileName[100];
unsigned long word = 1720;

int main(){
    openlog("writer-log", LOG_PID, LOG_USER);

    syslog(LOG_INFO, "Start logging");
    
    printf("Enter full path to filename: \n");
    scanf("%s", fileName);

    printf("Enter text: \n");
    scanf("%ld", word);

    fd = open (fileName, O_RDWR | O_CREAT);

    
    size_t count;
    ssize_t nr;

    count = sizeof (word);
    nr = write (fd, &word, count);

    syslog(LOG_DEBUG, "Writing %ld to %s", word, fileName);
    
}