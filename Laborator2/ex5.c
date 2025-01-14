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
#define ALARM_TIME 3

int count;
char fisier[BUFFER_SIZE];
__sighandler_t sigIntValue;

void handleSIGUSR1(int semnal)
{
     int fd = open(fisier, O_WRONLY | O_CREAT, 0777);
     if(fd == -1) {
         if(errno != EEXIST) { 
            printf("Eroare la deschiderea fisierului %s!\n", fisier);
            exit(1);
         }
     }

   write(fd, "Am primit semnal\n", 18);
   close(fd);
}

void handleSIGINT(int semnal)
{
   
}

void handleSIGALRM(int semnal)
{
      count++;
      if(count == 20) 
         signal(SIGINT, sigIntValue);
}

int main(int arg, char * argv[])
{
      signal(SIGUSR1, handleSIGUSR1);
      sigIntValue = signal(SIGINT, handleSIGINT);
      signal(SIGALRM, handleSIGALRM);
      while(1)
      {     
            printf("Procesul cu PID-ul %d este la a %d afisare!\n", getpid(), count + 1);      
            printf("Alarma trimisa!\n");
            alarm(ALARM_TIME);
            sigset_t set;
            sigfillset(&set);
            sigdelset(&set, SIGALRM);
            sigsuspend(&set);
      }

      exit(0);
}