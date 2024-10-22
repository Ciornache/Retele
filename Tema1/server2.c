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
#include <sys/socket.h>
#include <utmp.h>

#define BUFFER_SIZE 4096
#define PATH_SIZE 100

char command[BUFFER_SIZE], response[BUFFER_SIZE];
char buffer[BUFFER_SIZE];

void removeProcess(const char * id, int fd);
int checkIfInformationIsValid(const char * info, const char * fileName);
void handleLogin(const char * username, const char * id);
void handleGetLoggedUsers(const char * id, int sfd);
void handleGetProcInfo(const char * pid, const char * id, int sfd);
void handleLogOut();
void handleCommand(char * command, int sfd[]);
void getProcInfo(const char * id, int sfd);

int main(int arg, char * argv[])
{
    int bytes_read;
    int sfd[2];

    if(mkfifo("client_to_server.fifo", 0777) == -1)
    {
        if(errno != EEXIST) 
        {
            perror("Server: Eroare la crearea fifo-ului intre server si client!\n");
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

    if(creat("processes.txt", 0777) == -1)
    {
        if(errno != EEXIST)
        {
            perror("Server: Eroare la crearea fisierului processes.txt\n");
            exit(8);
        }
    }

    if(truncate("processes.txt", BUFFER_SIZE) == -1)
    {
        perror("Server: Eroare la trunchiere!\n");
        exit(9);
    }

    int fdr = open("client_to_server.fifo", O_RDONLY);
    if(fdr == -1)
    {
        perror("Server: Eroare la deschiderea capatului de citire a fifo-ului dintre client si server!\n");
        exit(3);
    }

    while((bytes_read = read(fdr, command, sizeof(command))) > 0)
    {
        ///write(1, command, bytes_read);
        command[bytes_read] = 0;

        if(socketpair(AF_LOCAL, SOCK_STREAM, 0, sfd) == -1)
        {
            perror("Server: Eroare la crearea socket pair-ului pentru comunicare cu childrenii!\n");
            exit(5);
        }
        
        int fid = fork();
        if(fid != 0)
        {

            int j = strlen(command) - 2;
            while(command[j] >= '0' && command[j] <= '9')
                j--;
            
            char id[20], username[101];
            strcpy(id, command + j + 1);
            id[strlen(command) - 2 - j] = 0;

            char path[PATH_SIZE];
            strcpy(path, "server_to_client");
            strcat(path, id); strcat(path, ".fifo");
            ///printf("%s\n", path); 

            int fdsc = open(path, O_WRONLY);
            if(fdsc == -1)
            {
                perror("Eroare la deschiderea fifo-ului de comunicare intre server si client!\n");
                exit(10);
            }

            close(sfd[0]);
            waitpid(fid, NULL, 0);
            char response[BUFFER_SIZE];
            int bytes_read;
            while((bytes_read = read(sfd[1], response, sizeof(response))) > 0)
            {
                response[bytes_read] = 0;
                write(fdsc, response, strlen(response));
            }  
            close(sfd[1]);
            close(fdsc);
        }
        else 
        {
            command[bytes_read] = 0;
            handleCommand(command, sfd);
            exit(0);
        }
        fflush(NULL);
    }
    close(sfd[1]);
    close(fdr);
    exit(0);
}


int checkIfInformationIsValid(const char * info, const char * fileName)
{

    int fd = open(fileName, O_RDONLY);
    if(fd == -1) 
    {
        perror("Server: Eroare la deschiderea fisierului processes.txt\n");
        exit(5);
    }

    char name[BUFFER_SIZE];
    int j = 0;
    int bytes_read;
    while((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        j = 0;
        int i;
        for(i = 0; i < bytes_read; i++)
            if(buffer[i] != ' ')
                name[j++] = buffer[i];
            else {
                name[j] = 0;
                ///printf("%s %s\n", name, info);
                if(strcmp(name, info) == 0)
                    return 1;
                j = 0;
            }
    }
    name[j] = 0;
    if(strcmp(name, info) == 0)
            return 1;
    close(fd);
    return 0;
}

void handleLogin(const char * username, const char * id)
{
    int info = checkIfInformationIsValid(id, "processes.txt");
    if(info == 0) 
    {
        if(checkIfInformationIsValid(username, "users.txt") == 1)
        {
            int fdp = open("processes.txt", O_WRONLY | O_APPEND);
            if(fdp == -1)
            {
                perror("Server: Eroare la deschiderea fisierului processes.txt!\n");
                exit(9);
            }
            printf("Server: Am conectat client-ul cu id-ul %s la server\n", id);
            char id2[20];
            strcpy(id2, id);
            int s = strlen(id);
            id2[s] = ' ';
            id2[s + 1] = 0;
            ///printf("%s\n", id2);
            write(fdp, id2, s + 1);
            close(fdp);
            fflush(NULL);
        }
        else 
            printf("Server: User-ul %s nu este autorizat sa acceseze server-ul!\n", username);
    }
    else 
        printf("Server: Client-ul cu id-ul %s este deja conectat la server!\n", id);
    
}

void handleLogOut(const char * id)
{
    if(checkIfInformationIsValid(id, "processes.txt") == 1) 
    {
        int fd = open("processes.txt", O_WRONLY);
        removeProcess(id, fd);
        printf("Server: Am deconectat procesul cu id-ul %s de la server!\n", id);
    }
    else {
        printf("Server: Procesul cu id-ul %s nu este conectat la server!\n", id);
    }
}

void removeProcess(const char * id, int fd)
{
    ftruncate(fd, BUFFER_SIZE);
    char name[BUFFER_SIZE];
    int j = 0;
    int bytes_read;
    while((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        j = 0;
        int i;
        for(i = 0; i < bytes_read; i++)
            if(buffer[i] != ' ')
                name[j++] = buffer[i];
            else {
                name[j] = 0;
                if(strcmp(name, id) != 0) 
                {
                    write(fd, name, strlen(name));
                    write(fd, " ", 1);                   
                }
                j = 0;
            }
    }
}

void getProcInfo(const char * pid, int sfd)
{
    char path[BUFFER_SIZE];
    strcpy(path, "/proc/"); strcat(path, pid); strcat(path, "/status");
    int fd = open(path, O_RDONLY);
    if(fd == -1)
    {
        write(sfd, "0", 1);
        printf("Server: Eroare la deschiderea fisierului cu informatii despre procesul cu id-ul %s!\n", pid);
        return;
    }

    char ch;
    int bytes_read, j = 0, ok = 0;
    char label[BUFFER_SIZE], info[BUFFER_SIZE];
    char response[BUFFER_SIZE], message[BUFFER_SIZE];
    const char * desiredLabels[5] = {"PPid", "Uid", "VmSize", "Name", "State"};

    while((bytes_read = read(fd, &ch, sizeof(char))) > 0)
    {
        if(ch == ':')
        {
            label[j] = 0;
            for(int f = 0;f < 5; f++) {
                if(strcmp(label, desiredLabels[f]) == 0)
                    ok = f + 1;
            }
            j = 0;
        }
        else if(ch == '\n')
        {
            info[j] = 0;
            if(ok) {
                strcat(response, desiredLabels[ok - 1]);
                strcat(response, ":");
                strcat(response, info);
                strcat(response, "\n");
            }
            ok = j = 0;
        }
        else 
        {
            if(ok) info[j++] = ch;
            else label[j++] = ch;
        }
    }

    snprintf(message, sizeof(strlen(response)), "%d ", strlen(response));
    strcat(message, response);
    write(sfd, message, strlen(message));

    printf("Server: Am terminat de procesat comanda get-proc-info pentru id-ul %s!\n", pid);

}

void handleGetProcInfo(const char * pid, const char * id, int sfd)
{
    if(checkIfInformationIsValid(id, "processes.txt") == 1)
    {
        printf("Server: Comanda aprobata! User-ul este logat. Procesez comanda...\n");
        getProcInfo(pid, sfd);
    }
    else 
    {
        write(sfd, "0", 1);
        printf("Server: Comanda nu a putut fi executata. Procesul cu id-ul %s nu este conectat la server!\n", id);
    }
}

void handleGetLoggedUsers(const char * id, int sfd)
{
    if(checkIfInformationIsValid(id, "processes.txt") == 1) 
    {
        printf("Server: Procesez comanda get-logged-users...\n");
        struct utmp * info;
        char response[BUFFER_SIZE], number[BUFFER_SIZE], message[BUFFER_SIZE];
        while((info = getutent()) != NULL)
        {
            strcat(response, "User: ");
            strcat(response, info->ut_user);
            strcat(response, "\nHostname: ");
            strcat(response, info->ut_host);
            strcat(response, "\nTime entry was made: ");
            snprintf(number, sizeof(number), "%d\n", info->ut_tv.tv_sec);
            strcat(response, number);
        }
        snprintf(message, sizeof(strlen(response)), "%d ", strlen(response));
        strcat(message, response);
        write(sfd, message, strlen(message));
        printf("Server: Am procesat comanda get-logged-users!\n");
    }
    else 
    {
        write(sfd, "0", 1);
        printf("Server: Comanda nu a putut fi executata. Procesul cu id-ul %s nu este conectat la server!\n", id);
    }
}

void handleCommand(char * command, int sfd[])
{
    close(sfd[1]); 
    ///printf("%s\n", command);
    int j = strlen(command) - 2;
    while(command[j] >= '0' && command[j] <= '9')
        j--;
    
    char id[20], username[101];
    strcpy(id, command + j + 1);
    id[strlen(command) - 2 - j] = 0;

    for(int i = 0;command[i]; i++)
        if(command[i] == ':')
        {
            i++;
            int k = 0;
            while(i <= j) {
                if(command[i] != ' ' && command[i] != '\n')
                    username[k++] = command[i++];
                else i++;
            }
            username[k] = 0;
            break;
        }

    // printf("%s\n", username);

    const char * prefix = strtok(command, " :0123456789\n");
    ///printf("%s\n", prefix);
    if(strcmp(prefix, "login") == 0)
    {
        handleLogin(username, id);
        write(sfd[0], "0", 1);
    }
    else if(strcmp(prefix, "get-logged-users") == 0) 
        handleGetLoggedUsers(id, sfd[0]);
    else if(strcmp(prefix, "get-proc-info") == 0) 
        handleGetProcInfo(username, id, sfd[0]);
    else if(strcmp(prefix, "logout") == 0) 
    {
        handleLogOut(id);
        write(sfd[0], "0", 1);
    }
    else if(strcmp(prefix, "quit") == 0)
    {
        write(sfd[0], "0", 1);
        printf("Server: Clientul cu id-ul %s s-a deconenctat!\n", id);
    }
    close(sfd[0]);
}