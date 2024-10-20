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

char command[BUFFER_SIZE], response[BUFFER_SIZE];

int main(int arg, char * argv[])
{

    int bytes_read;

    if(mkfifo("client_to_server.fifo", 0777) == -1) {
        if(errno != EEXIST) 
        {
            perror("Client: Eroare la crearea fifo-ului!\n");
            exit(1);
        }
    }

    if(mkfifo("server_to_client.fifo", 0777) == -1)
    {
        if(errno != EEXIST)
        {
            perror("Server: Eroare la crearea fifo-ului intre server si client!\n");
            exit(2);
        }
    }
    
    int fdw = open("client_to_server.fifo", O_WRONLY);
    if(fdw == -1)
    {
        perror("Client: Eroare la deschiderea capatului de scriere a fifo-ului!\n");
        exit(4);
    }

    int fdsc = open("server_to_client.fifo", O_RDONLY);
    if(fdsc == -1)
    {
        perror("Client: Eroare la deschiderea canalului de comunicatie intre server si client!\n");
        exit(5);
    }

    int ok = 0;

    while((bytes_read = read(0, command, sizeof(command))) > 0 && !ok)
    {
        command[bytes_read] = 0;
        if(strcmp(command, "quit") == 0) 
            ok = 1;

        snprintf(command + bytes_read, 10, "%d ", getpid());
        ///printf("%s %d\n", command, strlen(command));
        write(fdw, command, strlen(command));

        int pipe_fd[2];
        if(pipe(pipe_fd) == -1)
        {
            perror("Client: Eroare la crearea unui canal anonim de comunicatie!\n");
            exit(7);
        }

        int fid = fork();
        if(fid == -1)
        {
            perror("Client: Eroare la crearea unu proces fiu!\n");
            exit(8);
        }

        if(fid != 0)
        {
            close(pipe_fd[1]);
            waitpid(fid);
            char response[BUFFER_SIZE];
            int bytes_read;
            while((bytes_read = read(pipe_fd[0], response, sizeof(response))) > 0)
            {
                response[bytes_read] = 0;
                printf("%s", response);
            }  
            close(pipe_fd[0]);
        }
        else 
        {
            close(pipe_fd[0]);
            int bytes_read = read(fdsc, response, sizeof(response));
            if(response[0] == '0')
            {
                close(pipe_fd[1]);
                exit(0);
            }
            int i = 0, d = 0;
            while(response[i] >= '0' && response[i] <= '9')
                i++;
            i++;
            char parentMessage[BUFFER_SIZE];
            for(int j = i; j < bytes_read; j++)
                parentMessage[d++] = response[j];
            parentMessage[d] = 0;
            write(pipe_fd[1], parentMessage, sizeof(parentMessage));
            close(pipe_fd[1]);
            exit(0);
        }

        fflush(NULL);
    }
    
   
    close(fdsc);
    close(fdw);

    exit(0);

}