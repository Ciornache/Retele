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
char user[10][BUFFER_SIZE];
int lin = 0;

int main()
{
    FILE  * fd = fopen("/etc/passwd", "r");
    if(fd == NULL) 
    {
        perror("Eroare la deschiderea fisierului /etc/passwd!\n");
        exit(1);
    }

    int id = getuid();
    char sid[BUFFER_SIZE];
    sprintf(sid, "%d", id);
    printf("%s\n", sid);
    while(fscanf(fd, "%s", line))
    {
        printf("HERE %s", line);
        char * p = strtok(line, ":");
        while(p)
        {
            strcpy(user[lin++], p);
            p  = strtok(NULL, ":");
        }
        lin--;
        if(strcmp(sid, user[2]) == 0) {
            printf("The current user %s is using %s shell\n", user[0], user[6]);
        }
    }

    exit(0);
}