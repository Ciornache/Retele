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
     int pipe_fd[2];
     int pipe_fd2[2];
     if(pipe(pipe_fd) == -1) {
        perror("Eroare la crearea pipe-ului anonim!\n");
        exit(1);
     }
     if(pipe(pipe_fd2) == -1) {
        perror("Eroare la crearea unui canal anonim!\n");
        exit(2);
     }
     int id = fork();
     if(id) {
        close(pipe_fd[READ]);
        while(1) 
        {
            scanf("%s", buffer);
            if(strlen(buffer) == 1)
                break;
            write(pipe_fd[WRITE], buffer, sizeof(buffer));
            int len_read = read(pipe_fd2[READ], buffer, sizeof(buffer));
            printf("%s\n", buffer);
        }
        close(pipe_fd2[WRITE]);
        close(pipe_fd[READ]);
     }
     else {
        close(pipe_fd[WRITE]);
        close(pipe_fd2[READ]);
        while(read(pipe_fd[READ], buffer, sizeof(buffer)) > 0)
        {
            for(int i = 0; buffer[i]; i++)
                if(buffer[i] >= 'a' && buffer[i] <= 'z')
                    buffer[i] = buffer[i] - 'a' + 'A';
                else 
                    buffer[i] = buffer[i] - 'A' + 'a';
            write(pipe_fd2[WRITE], buffer, sizeof(buffer));
        }
     }
     exit(0);
}