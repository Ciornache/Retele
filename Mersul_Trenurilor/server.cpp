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
#include <sstream>
#include <mutex>
#include <tinyxml2.h>
#include "train.h"
#include "database.h"
#include <condition_variable>

using namespace tinyxml2;


#define PORT_NUMBER 1311
#define BUFFER_SIZE 1000
#define MAX_SCHEDULE_SIZE 11900

#define NUMBER_OF_CONCURRENT_CLIENTS 5

const static std::vector<std::string> days = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

struct sockaddr_in server, from;
int sock, client;
TimeStamp globalTime;
bool isNotifyOn = false;

char msg[BUFFER_SIZE];
char response[BUFFER_SIZE];

std::mutex m;
std::mutex del_command;
std::mutex delay_on;
std::vector<std::thread> threads;
std::vector<Road*> roads;
std::vector<Train*> trains;
std::map<int, bool> isThreadActive;
std::condition_variable cv;

void update_timestamp();
void updateTrainDelay();
void handleError(std::string message, int exit_code);
Road * findRoadById(int road_id);
void fetchRoad(XMLElement * element);
void fetchTrainData(XMLElement * train);
void fetchData(std::string path, std::string header);
void handleClient(int client);
std::pair<bool, std::vector<std::string>> validateCommand(std::string command, std::string header);
Train* findTrainById(int train_id);
std::string handle_delay(std::string header, std::vector<std::string> args);
std::string handle_getSchedule(std::string header, std::vector<std::string> args);
std::string handle_nextHourArrivals();
std::string handle_nextHourDepartures();
std::string handle_addNotification(int train_id, int client_id);
std::string handle_login(std::string name, std::string password);
std::string handle_register(std::string name, std::string password, int user_id);


void handleCommand(std::string command, std::string header, std::vector<std::string> args, int client);
void createTableUsers(std::string header);
void createTableNotifications(std::string header);
void createTableClients(std::string header);
void notifyClient(int client, int client_id, std::string header);

bool insert_user(std::string name, std::string password);

int main() 
{
    int pid = getpid();
    std::string header = "Server(" + std::to_string(pid) + "): ";
    int sfd;

    if((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        handleError(header + "Socket function failed!", 2);

    memset(&server, 0, sizeof(server));
    memset(&from, 0, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT_NUMBER);

    if (bind(sfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
        handleError(header + "Error at binding the server to a local ip_adress and port!\n", 3);

    if (listen(sfd, NUMBER_OF_CONCURRENT_CLIENTS) == -1) 
        handleError(header + "Error at setting the listening maximum to " + std::to_string(NUMBER_OF_CONCURRENT_CLIENTS), 4);

    std::cout << header << "Waiting at port " << PORT_NUMBER << '\n';
    std::cout << header << "Fetching train data..." << '\n';

    fetchData("/home/ciornei/Mersul_Trenurilor/schedule.xml", header);
    std::cout << header << "Fetched data!\n";
    std::cout << header << "Initializing database...\n";

    Database database("users.db");
    createTableUsers(header);
    database.clearTable("notifications");
    createTableNotifications(header);
    database.clearTable("clients");
    createTableClients(header);

    std::cout << header << "Initialized database!\n";

    threads.push_back(std::thread(update_timestamp));

    while (1) 
    {
        socklen_t length = sizeof(from);
        client = accept(sfd, (struct sockaddr *)&from, &length);

        if (client < 0) 
            handleError(header + "Not able to accept the client", 5);
        std::cout << header << "A client connected to the server!\n";
        threads.push_back(std::thread(handleClient, client));
    }

    for(auto &thread : threads)
        if(thread.joinable())
            thread.join();

    close(sfd);

    return 0;
}

void update_timestamp()
{
   while(1) 
   {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        isNotifyOn = false;
        {
            std::unique_lock<std::mutex> lock(m);
            updateTrainDelay();
            globalTime++;
            isNotifyOn = true;
            cv.notify_all();
        }
   }
}

void updateTrainDelay()
{
    int day_id = globalTime.getDayIndex();
    for(auto train : trains)
    {
        /// Calculez delay-ul maxim care a fost submitted la momentul de timp time

        int max_delay = -1e9;
        for(auto delay : train->delays[globalTime.to_string()])
            max_delay = std::max(max_delay, delay);
        
        bool delay_add = false;
        int lastRouteDelay = 0, prevTotalTime = 0;
        TimeStamp startTime;
        ScheduleElement * lastRoute = NULL;

        for(auto route : train->schedule[day_id]) 
        {
            lastRoute = &route;
            if(delay_add)
                break;

            startTime = route.startTime;
            int total_time = 0;
            TimeStamp prevStartTime;
            Road * road = route.road;

            /// Actualizez ultimul delay submitted pentru fiecare drum 

            while(road != NULL)
            {
                if(road->last_delay > 0)
                    road->last_delay--;
                road = road->nextRoad;
            }

            /// Daca delay-ul maxim nu este cuprins in last_delay, atunci last_delay si delay cresc

            bool ok = false;
            road = route.road;
            total_time = 0;
            while(road != NULL && !ok)
            {
                total_time += road->time;
                
                if(startTime + total_time + road->delay > globalTime) 
                {                        
                    if(road->last_delay < max_delay)
                    {
                        int new_delay = max_delay - road->last_delay;
                        road->last_delay += new_delay; road->delay += new_delay;
                        Road * cpRoad = road->nextRoad;
                        while(cpRoad != NULL) 
                        {
                            cpRoad->delay += new_delay;
                            lastRouteDelay = cpRoad->delay;
                            cpRoad = cpRoad->nextRoad;
                        }
                        delay_add = true;
                        
                    }
                    ok = true;
                }
                road = road->nextRoad;
            }

            prevStartTime = route.startTime;
            prevTotalTime = total_time;

        }

        /// Daca cumva programul trenului activ va trece peste inceputul urmatorului tren, atunci actualizam inceputul urmatorului tren si schimbam delay-ul

        if(delay_add) 
        {
            if(startTime + prevTotalTime + lastRouteDelay > lastRoute->startTime) 
            {
                int dif = (startTime + prevTotalTime + lastRouteDelay) - lastRoute->startTime;
                lastRoute->startTime = startTime + prevTotalTime + lastRouteDelay;
                Road * road = lastRoute->road;
                while(road != NULL)
                {
                    road->delay += dif;
                    road = road->nextRoad;
                }
            }
        }

    }
}

void handleError(std::string message, int exit_code)
{
    fprintf(stderr, "%s\n", message.c_str());
    exit(exit_code);
}

Road * findRoadById(int road_id)
{
    for(unsigned int i = 0;i < roads.size(); i++)
        if(roads[i]->road_id == road_id)
            return roads[i];
    return NULL;
}

void fetchRoad(XMLElement * element)
{
    std::string road_id = element->Attribute("id");
    Road * road, *prevRoad = NULL, *roadRoot = NULL;
    XMLElement * segment = element->FirstChildElement();
    while(segment != NULL)
    {
        std::string to_station = segment->Attribute("to_station");
        std::string from_station = segment->Attribute("from_station");
        std::string time = segment->Attribute("time");
        road = new Road(std::stoi(road_id), 0, std::stoi(time), std::stoi(from_station), std::stoi(to_station));
        if(roadRoot == NULL) roadRoot = road;
        if(prevRoad != NULL)
            prevRoad->nextRoad = road, road->prevRoad = prevRoad;
        prevRoad = road;
        segment = segment->NextSiblingElement();
    }
    road->nextRoad = NULL; roadRoot->prevRoad = NULL;
    roads.push_back(roadRoot);
}

void fetchTrainData(XMLElement * train)
{
    std::string train_id = train->Attribute("id");
    XMLElement * road = train->FirstChildElement();
    XMLElement * schedule = road->NextSiblingElement();
    std::string road_id = road->Attribute("id");
    XMLElement * day_schedule = schedule->FirstChildElement();
    Train * trainObj = new Train(stoi(train_id), findRoadById(std::stoi(road_id)));
    while(day_schedule != NULL)
    {
        std::string day = day_schedule->Attribute("day");
        int day_id = TimeStamp::getDayIndex(day);
        XMLElement * direction = day_schedule->FirstChildElement("direction");
        while(direction != NULL)
        {
            bool isReversed = false;
            std::string type = direction->Attribute("type");
            std::string departure = direction->Attribute("departure");
            if(type == "reverse") 
                isReversed = true;
            trainObj->addToSchedule(day_id, isReversed, TimeStamp(departure));
            direction = direction->NextSiblingElement();
        }
        day_schedule = day_schedule->NextSiblingElement();
    }
    trains.push_back(trainObj);
}

void fetchData(std::string path, std::string header)
{
    XMLDocument doc;
    XMLError result = doc.LoadFile(path.c_str());
    if(result != XML_SUCCESS) 
        handleError(header + "Error at parsing the XML file!", 6);
    XMLElement * root = doc.RootElement();
    XMLElement * roads = root->FirstChildElement();
    XMLElement * firstRoad = roads->FirstChildElement();
    while(firstRoad != NULL)
    {
        fetchRoad(firstRoad);
        firstRoad = firstRoad->NextSiblingElement();
    }
    XMLElement * trains = roads->NextSiblingElement("trains");
    XMLElement * firstTrain = trains->FirstChildElement("train");
    while(firstTrain != NULL)
    {
        fetchTrainData(firstTrain);
        firstTrain = firstTrain->NextSiblingElement();
    }
}

void updateNotifications(int client, int client_id, std::string header)
{
    std::map<std::string, bool> check;
    while(isThreadActive[client_id]) 
    {
        {
            std::unique_lock<std::mutex> lock(m);
            cv.wait_for(lock, std::chrono::milliseconds(1000), []() -> bool {
                return isNotifyOn;
            });
            if(!check[globalTime.to_string()]) 
            {
                notifyClient(client, client_id, header);
                check[globalTime.to_string()] = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void handleClient(int client)
{
    int client_id = -1;
    auto id = std::this_thread::get_id();
    std::stringstream ss;
    std::string thread_id;
    ss << id; ss >> thread_id;
    std::string header = "Server(" + thread_id + "): ";
    std::thread * t;
    while(1)
    {
        char request[BUFFER_SIZE];
        memset(request, false, sizeof(request));
        int bytes_read;
        if((bytes_read = read(client, request, sizeof(request))) <= 0)
            handleError(header + "Unable to read from client!\n", 5);
        
        if(strlen(request) > 0) 
        {
            std::cout << header << "Received " << request << " from client!\n";
            std::string command(request);

            if(command.substr(0, 4) == "quit")
                break;

            std::pair<bool, std::vector<std::string>> valid = validateCommand(command, header);
            if(!valid.first) 
            {
                char response[BUFFER_SIZE];
                memset(response, 0, sizeof(response)); strcpy(response, "Invalid command");
                write(client, response, sizeof(response));
                continue;
            }
            client_id = std::stoi(valid.second.back());
            handleCommand(command, header, valid.second, client);
        }

        if(client_id != -1 && !isThreadActive[client_id]) 
        {
            t = new std::thread(updateNotifications, client, client_id, header);
            isThreadActive[client_id] = 1;
        }
        
    }

    if(client_id != -1)
        isThreadActive[client_id] = 0;
    
    delete t;
}

void notifyClient(int client, int client_id, std::string header)
{
    std::cout << "Notifying at " << globalTime.to_string() << '\n';
    Database database("users.db");
    std::vector<int> notifiableTrains = database.getNotifiableTrains(client_id);
    int day_id = globalTime.getDayIndex();
    std::string schedule;
    for(auto train_id : notifiableTrains)
    {
        for(auto train : trains)
        {
            if(train->train_id == train_id)
            {
                for(auto route : train->schedule[day_id])
                {
                    Road * road = route.road;
                    TimeStamp startTime = route.startTime;
                    int total_time = 0;
                    while(road != NULL)
                    {
                        total_time += road->time;
                        int dif = (startTime + total_time + road->delay) - globalTime;
                        if(dif == 5 || dif == 15 || dif == 30) 
                            schedule += "Train " + std::to_string(train_id) + " arrives in station " + std::to_string(road->to_station) + " in " + std::to_string(dif) + " minutes\n";
                        if(dif > 0)
                            break;
                        road = road->nextRoad;
                    }

                }
            }
        }
    }

    if(schedule.size() > 0) 
    {
        char response[MAX_SCHEDULE_SIZE];
        memset(response, false, sizeof(response));
        strcpy(response, schedule.c_str());
        if(write(client, response, sizeof(response)) <= 0)
        {
            std::cout << header << "Can't send notification to client\n";
            return;
        }
    }
}

std::pair<bool, std::vector<std::string>> validateCommand(std::string command, std::string header)
{
    std::pair<bool, std::vector<std::string>> answer;
    answer.first = true;
    std::vector<std::string> args;
    std::stringstream ss;
    std::string commandName, arg;
    ss << command; ss >> commandName;
    while(ss >> arg)
        args.push_back(arg);
    if(commandName == "get-schedule")
    {
        if(args.size() > 3)
            std::cout << header + "Too many arguments for command " + commandName << '\n';
    }
    else if(commandName == "add-delay")
    {
        if(args.size() != 3)
            std::cout << header + "Syntax " + commandName + " <train_id> <number_of_minutes>" << '\n';
    }
    else if(commandName == "next-hour-departures")
    {
        if(args.size() != 1)
            std::cout << header + "Command " + commandName + " doesn't require any arguments!" << '\n';
    }
    else if(commandName == "next-hour-arrivals")
    {
        if(args.size() != 1)
            std::cout << header + "Command " + commandName + " doesn't require any arguments!" << '\n';
    }
    else if(commandName == "register")
    {
        if(args.size() != 3)
            std::cout << header + "Command " + commandName + " syntax <username> <password>" << '\n';
    }
    else if(commandName == "login")
    {
        if(args.size() != 3)
            std::cout << header + "Command " + commandName + " syntax <username> <password>" << '\n';
    }
    else if(commandName == "help")
    {
        if(args.size() != 1)
            std::cout << header << "Command " + commandName + " doesn't require any arguments!\n";
    }
    else if(commandName == "add-notification")
    {
        if(args.size() != 2)
            std::cout << header << "Command " + commandName + " syntax <train_id>" << '\n';
    }
    else if(commandName == "logout")
    {
        if(args.size() != 1)
            std::cout << header << "Command " + commandName + " doesn't require any parameters" << '\n';
    }
    else 
        answer.first = false;
    
    answer.second = args;
    return answer;
}

Train* findTrainById(int train_id)
{
    for(unsigned int i = 0;i < trains.size(); i++)
        if(trains[i]->train_id == train_id)
            return trains[i];
    return NULL;
}

std::string handle_delay(std::string header, std::vector<std::string> args)
{
    args.pop_back();
    int train_id = std::stoi(args[0]);
    int delay = std::stoi(args[1]);
    Train * train = findTrainById(train_id);
    std::unique_lock<std::mutex> lock(m);
    train->addDelay(globalTime, delay);
    return "Update successful. Added delay to train " + std::to_string(train_id);
}

std::string handle_getSchedule(std::string header, std::vector<std::string> args)
{
    args.pop_back();
    int train_id = -1;
    std::string day;
    for(unsigned int i = 0;i < args.size(); i++) 
    {
        if(args[i][0] >= '0' && args[i][0] <= '9')
            train_id = std::stoi(args[i]);
        else
        {
            int day_id = TimeStamp::getDayIndex(args[i]);
            if(day_id != -1)
                day = args[i];
        }
    }

    bool isRoadWritten = false, isTrainValid = false, isDayValid = true;

    std::string schedule;
    schedule += "Current Time " + globalTime.to_string() + "\n";
    for(auto train : trains)
    {   
        if(train_id != -1 && train->train_id != train_id)
            continue;
        
        isTrainValid = true;    

        if(train_id == -1) 
            schedule += "Train " + std::to_string(train->train_id) + "\n";
        
        unsigned int first_day = 0, last_day = 6;
        if(day.size() != 0) 
        {
            first_day = last_day = TimeStamp::getDayIndex(day);
            if(first_day == -1)
                isDayValid = false;
        }
        while(first_day <= last_day)
        {
            if(day.size() == 0)
                schedule += days[first_day] + "\n";
            for(auto route : train->schedule[first_day])
            {
                Road * road = route.road;
                TimeStamp startTime = route.startTime;
                int total_time = 0;
                int from_station = road->from_station, to_station;
                while(road != NULL)
                {
                    total_time += road->time;
                    to_station = road->to_station;
                    road = road->nextRoad;
                }

                isRoadWritten = true;

                if(!route.isReversed) 
                {
                    schedule += "Route: From station " + std::to_string(from_station) + " to station " + std::to_string(to_station) + ".";
                    schedule += " Departure Time: " + startTime.to_string() + ".";
                    schedule += " Arrival Time: " + (startTime + total_time).to_string() + "\n";
                }
                else
                {
                    schedule += "Route: From station " + std::to_string(to_station) + " to station " + std::to_string(from_station) + ".";
                    schedule += " Departure Time: " + startTime.to_string() + ".";
                    schedule += " Arrival Time: " + (startTime + total_time).to_string() + "\n";
                }
            }
            first_day++;
        }
    }

    if(!isRoadWritten) 
    {
        if(!isTrainValid && train_id != -1)
            return "Error: Invalid train id " + std::to_string(train_id) + ". Please provide a train id between 1 and 10\n";
        if(!isDayValid && days.size() != 0) 
            return std::string("Error: Invalid day ") + day + "\n";
    }

    return schedule;
}

std::string handle_nextHourArrivals()
{
    std::string schedule;
    int day_id = globalTime.getDayIndex();
    schedule += "Current Time " + globalTime.to_string() + "\n";
    for(auto train : trains)
    {
        for(auto route : train->schedule[day_id])
        {
             Road * road = route.road;
             int total_time = 0;
             TimeStamp startTime = route.startTime;
             while(road != NULL)
             {
                total_time += road->time;                
                int minutes = (startTime + total_time + road->delay) - globalTime;
                if(minutes >= 0 && minutes <= 60)
                {                
                    schedule += "Train " + std::to_string(train->train_id) + " arrives in station " + std::to_string(road->to_station) + " in " + std::to_string(minutes) + " minutes. ";
                    if(road->delay == 0)
                        schedule += "All according to the plan. No delays\n";
                    else if(road->delay > 0)
                        schedule += "Delay: " + std::to_string(road->delay) + "\n";
                }
                road = road->nextRoad;
             }
        }
    }
    return schedule;
}

std::string handle_nextHourDepartures()
{
    std::string schedule;
    int day_id = globalTime.getDayIndex();
    schedule += "Current Time " + globalTime.to_string() + "\n";
    for(auto train : trains)
    {
        for(auto route : train->schedule[day_id])
        {
             Road * road = route.road;
             int total_time = 0;
             TimeStamp startTime = route.startTime;
             while(road != NULL)
             {
                int minutes = (startTime + total_time + road->delay) - globalTime;
                if(minutes >= 0 && minutes <= 60)
                {                
                    schedule += "Train " + std::to_string(train->train_id) + " leaves station " + std::to_string(road->from_station) + " in " + std::to_string(minutes) + " minutes. ";
                    if(road->delay == 0)
                        schedule += "All according to the plan. No delays\n";
                    else if(road->delay > 0)
                        schedule += "Delay: " + std::to_string(road->delay) + "\n";
                }
                total_time += road->time;                
                road = road->nextRoad;
             }
        }
    }
    return schedule;
}

std::string handle_register(std::string name, std::string password, int user_id)
{
    Database database("users.db");
    bool ok = database.insertUser(name, password, user_id);
    if(ok) 
        return "Account created for user " + name + "\n";
    else 
        return "Account already exists for user " + name + "\n";
}

std::string handle_login(std::string name, std::string password, int client_id)
{
    Database database("users.db");
    bool ok = database.validateUser(name, password);
    if(!ok)
        return "Login failed. Invalid user or password!\n";
    
    std::pair<bool, bool> response = database.isUserLoggedIn(client_id);

    if(response.second)
        return "Login failed. User already logged into the account!\n";

    if(!response.first)
       database.insertClient(client_id, 1);
    else
       database.updateUser(client_id, 1);

    return "Client " + std::to_string(client_id) + " connected to the account!\n";
}

std::string handle_addNotification(int train_id, int client_id)
{
    Database database("users.db");
    std::pair<bool,bool> ok = database.isUserLoggedIn(client_id);
    if(!ok.second)
        return "Error: Log into an account to add a notification!\n";
    database.insertNotification(train_id, client_id);
    return "Added notification for train " + std::to_string(train_id) + " succesfully!\n";
}

std::string handle_logout(int client_id)
{
    Database database("users.db");
    std::pair<bool, bool> response = database.isUserLoggedIn(client_id);
    if(response.second)
    {
        database.updateUser(client_id, 0);
        return "Logged out!\n";
    }
    else 
        return "Error: Cannot logout user. User is not logged in!\n";
}

void handleCommand(std::string command, std::string header, std::vector<std::string> args, int client) 
{
    std::stringstream ss;
    std::string command_name, res;
    ss << command; ss >> command_name;
    if(command_name == "add-delay") 
        res = handle_delay(header, args);
    else if(command_name == "get-schedule")
        res = handle_getSchedule(header, args);
    else if(command_name == "next-hour-arrivals")
        res = handle_nextHourArrivals();
    else if(command_name == "next-hour-departures")
        res = handle_nextHourDepartures();
    else if(command_name == "register")
        res = handle_register(args[0], args[1], std::stoi(args[2]));
    else if(command_name == "login")
        res = handle_login(args[0], args[1], std::stoi(args[2]));
    else if(command_name == "add-notification")
        res = handle_addNotification(std::stoi(args[0]), std::stoi(args[1]));
    else if(command_name == "logout")
        res = handle_logout(std::stoi(args[0]));
    char response[MAX_SCHEDULE_SIZE];
    memset(response, false, sizeof(response));
    strcpy(response, res.c_str());
    if(write(client, response, sizeof(response)) <= 0)
    {
        std::cout << header << "Can't send to client the response to command " << command_name << '\n';
        return;
    }
}

void createTableUsers(std::string header)
{
    Database database("users.db");
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            password TEXT NOT NULL
        );
    )";
    int ok = database.createTable(sql);
    if(ok != 0)
        std::cout << header << "Error: Not able to create table users!\n";
}

void createTableNotifications(std::string header)
{
    Database database("users.db");
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS notifications (
            client_id INTEGER PRIMARY KEY,
            train_id INTEGER NOT NULL
        );
    )";

    int ok = database.createTable(sql);
    if(ok != 0)
        std::cout << header << "Error: Not able to create table notifications!\n";
}

void createTableClients(std::string header)
{
    Database database("users.db");

    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS clients (
            client_id INTEGER PRIMARY KEY,
            logged INTEGER NOT NULL
        );
    )";

    int ok = database.createTable(sql);
    if(ok != 0)
        std::cout << header << "Error: Not able to create table logged!\n";
}