#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <condition_variable>
#include <fstream>

#define BUFFER_SIZE 11900

std::mutex m;
std::condition_variable cv;

std::ofstream g;

bool mainRunning = true;
bool enterCommand = false;

void handleError(std::string message, int exit_code)
{
    fprintf(stderr, "%s\n", message.c_str());
    exit(exit_code);
}

void waitForAnswers(int sfd, std::string header)
{
    char response[BUFFER_SIZE];
    while(mainRunning)
    {
        if(read(sfd, response, BUFFER_SIZE) < 0)
            handleError(header + "Couldn't read from server the response!", 5);
        
        if(!strncmp(response, "Notification", 12)) 
        {
            strcpy(response, response + 14);
            g << response;
            g.flush();
            continue;
        }

        if(strlen(response) > 0) 
        {
            std::cout << header << "Received from server: " << response;
            if(!strncmp(response, "quit", 4))
                mainRunning = false;
            enterCommand = true;
            cv.notify_all();
        }
    }
}

int sfd, port;
char message[BUFFER_SIZE], response[BUFFER_SIZE];
std::string command;

int main(int arg, char * argv[])
{
    struct sockaddr_in  server;

    int pid = getpid();

    std::string filename = "Client-" + std::to_string(pid) + ".txt";
    g.open(filename.c_str());

    std::string header = "Client(" + std::to_string(pid) + "): ";
    if(arg < 3)
       handleError(header + "Syntax: <ip-address> <port>", 1);

    std::string ip_adress(argv[1]);

    port = atoi(argv[2]);
    if((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        handleError(header + "Socket function failed!", 2);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip_adress.c_str());
    server.sin_port = htons(port);

    if(connect(sfd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1)
        handleError(header + "Connect function failed!", 3);
    
    std::thread t1 = std::thread(waitForAnswers, sfd, header);
    std::cout << header << "Connection established succesfully!\n";

    while(mainRunning)
    {                 
        std::cout << header << "Enter command: ";
        std::getline(std::cin, command);
        enterCommand = false;
        command = command + " " + std::to_string(pid);
        memset(message, false, sizeof(message));
        strcpy(message, command.c_str());
        if(write(sfd, message, strlen(message)) < 0)
            handleError(header + "Couldn't write to server the command " + command, 4);
        std::unique_lock<std::mutex> lock(m);
        cv.wait_for(lock, std::chrono::seconds(30), []() -> bool {
            return enterCommand;
        });
    }

    if(t1.joinable())
        t1.join();

    close(sfd);
    return 0;
}