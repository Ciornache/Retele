#include<stdio.h> 

#include<stdlib.h> 

#include<unistd.h> 

#include<sys/types.h> 

#include<string.h> 

#include<sys/wait.h> 

#define READ 0

#define WRITE 1 

int main() { 

    int pipe_fds[2];  

    pipe(pipe_fds);

    pid_t pid = fork(); 

    if(pid > 0) { // parent

        write(pipe_fds[WRITE], "stuff", 5);

        sleep(3);

        close(pipe_fds[WRITE]);

        char buff[10];
        int len_read = read(pipe_fds[READ], buff, 100);
        buff[len_read] = 0;
        printf("Read from child %s\n", buff);

    } else { // child

        char buff[10];

        int len_read = read(pipe_fds[READ], buff, 100);

        buff[len_read] = '\0';

        close(pipe_fds[READ]);

        printf("Read from parent: %s\n", buff);

        write(pipe_fds[WRITE], "stuff2", 6);

    }

} 