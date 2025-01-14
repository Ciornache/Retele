#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>
#include <time.h>

#define BUFFER_SIZE 4096

char line[BUFFER_SIZE];
char s1[BUFFER_SIZE];
char s2[BUFFER_SIZE];
char s3[BUFFER_SIZE * 2];

int main(int arg, char * argv[])
{
    if(arg < 2) 
    {
        perror("Insuficiente fisiere la linia de comanda!\n");
        exit(1);
    }

    int fd = open(argv[1], O_RDONLY | O_CREAT, 0777);
    if(fd == -1) 
    {
        if(errno != EEXIST) {
            perror("Eroare la deschiderea fisierului de intrare!\n");
            exit(2);
        }
    }

    read(fd, line, sizeof(line));
    char *p = strtok(line, " ");
    strcpy(s1, p);
    p = strtok(NULL, " ");
    strcpy(s2, p);

    close(fd);

    strcpy(s3, s1);
    strcat(s3, s2);

    int fd2 = open(argv[2], O_WRONLY | O_CREAT, 0777);

    if(fd2 == -1) {
        if(errno != EEXIST) 
        {
            perror("Eroare la deschiderea fisierului de iesire!\n");
            exit(3);
        }
    }
    write(fd2, s3, sizeof(s3));
    close(fd2);
    exit(0);
}