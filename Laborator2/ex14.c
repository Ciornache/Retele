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
#define READ 0
#define WRITE 1

char buffer[BUFFER_SIZE];

int main(int arg, char * argv[])
{
    if(mkfifo("canal.fifo", 0777) == -1) {
        if(errno != EEXIST) 
            perror("Canalul deja exista");
    }
    int fd = open("canal.fifo", O_WRONLY);
    if(fd == -1) {
        perror("Eroare la deschiderea fisierului!\n");
        exit(1);
    }
    while(scanf("%s", buffer))
    {
        printf("Am citit %s din procesul cu pid-ul %d\n", buffer, getpid());
        write(fd, buffer, sizeof(buffer));
    }
}