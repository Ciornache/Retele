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
#include "helpmessages.h"

using namespace tinyxml2;

#define MIN_ALARM_TIME 0
#define MAX_ALARM_TIME 60
#define PORT_NUMBER 1311
#define BUFFER_SIZE 1000
#define MAX_SCHEDULE_SIZE 30000
#define XML_PATH "/home/ciornei/Mersul_Trenurilor/schedule.xml"

#define NUMBER_OF_CONCURRENT_CLIENTS 5

const static std::vector<std::string> days = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

struct sockaddr_in server, from;
int sock, client;
TimeStamp globalTime;
bool isNotifyOn = false;

char msg[BUFFER_SIZE];
char response[BUFFER_SIZE];

std::mutex m, delay_on, del_command;
std::vector<std::thread> threads;
std::vector<std::shared_ptr<Road>> roads;
std::vector<Train*> trains;
std::map<int, bool> isThreadActive;
std::map<std::string, std::string> scheduleCache;
std::condition_variable cv;


void handleError(std::string message, int exit_code);
void handleClient(int client);

/// Validate Command \\\

std::pair<bool, std::vector<std::string>> validateCommand(std::string command, std::string header);

void notifyClient(int client, int client_id, std::string header);

/// Find By Id Functions \\\

std::shared_ptr<Road> findRoadById(int road_id);
Train* findTrainById(int train_id);

/// Time and Delay update \\\

void update_timestamp();
void updateTrainDelay();

/// Fetch Data from XML  \\\ 

void fetchRoad(XMLElement * element);
void fetchTrainData(XMLElement * train);
void fetchData(std::string path, std::string header);

/// Command Handlers \\\

void handleCommand(std::string command, std::string header, std::vector<std::string> args, int client);
std::string handle_delay(std::string header, std::vector<std::string> args);
std::string handle_getSchedule(std::string header, std::vector<std::string> args);
std::string handle_nextHourArrivals(int client_id);
std::string handle_nextHourDepartures(int client_id);
std::string handle_addAlarm(int train_id, int client_id);
std::string handle_login(std::string name, std::string password);
std::string handle_register(std::string name, std::string password, int user_id);
std::string handle_help();
std::string handle_quit(int client_id);
std::string handle_disableAlarm(int train_id, int client_id);
std::string handle_setLocation(int station_id, int client_id);

/// Delete client files : ls | grep -e "Client-" | xargs rm

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

    std::cout << header << "Fetching train data..." << '\n';

    fetchData(XML_PATH, header);
    std::cout << header << "Fetched data!\n";
    std::cout << header << "Initializing database...\n";

    Database::initDatabase(header);

    std::cout << header << "Initialized database!\n";
    std::cout << header << "Waiting at port " << PORT_NUMBER << '\n';

    threads.push_back(std::thread(update_timestamp));

    while (1) 
    {
        socklen_t length = sizeof(from);
        client = accept(sfd, (struct sockaddr *)&from, &length);
        if (client < 0) 
        { 
            std::cout << header << "Not able to accept the client\n";
            continue;
        }
        std::cout << header << "A client connected to the server!\n";
        threads.push_back(std::thread(handleClient, client));
    }

    for(auto &thread : threads)
        if(thread.joinable())
            thread.join();

    close(sfd);

    for(unsigned int j = 0;j < trains.size(); j++)
        delete trains[j];
    trains.clear();

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

        int max_delay = 0, pos_delay = 0, neg_delay = 0;
        for(auto delay : train->delays[globalTime.to_string()]) 
        {
            if(delay > 0)
                pos_delay = std::max(pos_delay, delay);
            else 
            {
                if(!neg_delay) neg_delay = delay;
                else neg_delay = std::min(neg_delay, delay);
            }
        }
        max_delay = pos_delay + neg_delay;
        bool delay_add = false;
        int lastRouteDelay = 0, prevTotalTime = 0;
        TimeStamp startTime;
        ScheduleElement * lastRoute = NULL;
        for(unsigned int idx = 0;idx < train->schedule[day_id].size(); idx++)
        {
            ScheduleElement route = train->schedule[day_id][idx];
            if(delay_add)
                break;
            if(idx + 1 < train->schedule[day_id].size())
                lastRoute = &train->schedule[day_id][idx + 1];
            startTime = route.startTime;
            int total_time = 0;
            TimeStamp prevStartTime;
            std::shared_ptr<Road> road = route.road;
            if(route.isReversed)
            {
                while(road->nextRoad != NULL)
                    road = road->nextRoad;
            }

            /// Actualizez ultimul delay submitted pentru fiecare drum 

            while(road != NULL)
            {
                if(road->last_delay > 0)
                    road->last_delay--;
                else if(road->last_delay < 0)
                    road->last_delay++;
                if(!route.isReversed)
                     road = road->nextRoad;
                else 
                     road = road->prevRoad;
            }

            /// Daca delay-ul maxim nu este cuprins in last_delay, atunci last_delay si delay cresc

            bool ok = false;
            road = route.road;
            total_time = 0;

            if(route.isReversed)
            {
                while(road->nextRoad != NULL)
                    road = road->nextRoad;
            }

            while(road != NULL && !ok)
            {
                total_time += road->time;
                
                if(startTime + total_time + road->delay > globalTime) 
                {                        
                    int remTimeUnits = (startTime + total_time + road->delay) - globalTime;
                    if(max_delay < 0 && -max_delay > remTimeUnits)
                        max_delay = -remTimeUnits;
                    
                    bool isDelayChanged = false;
                    int new_delay = 0;
                    if(road->last_delay <= 0 && max_delay > 0 || road->last_delay >= 0 && max_delay < 0)
                        new_delay = max_delay, road->last_delay += max_delay, road->delay += max_delay, isDelayChanged = true;
                    else 
                    {
                        if(road->last_delay >= 0 && max_delay > road->last_delay)
                        {
                            new_delay = max_delay - road->last_delay;
                            road->last_delay += new_delay; road->delay += new_delay;
                            isDelayChanged = true;
                        }
                        else if(road->last_delay <= 0 && -max_delay > -road->last_delay)
                        {
                            new_delay = max_delay - road->last_delay;
                            road->last_delay += new_delay; road->delay += new_delay;
                            isDelayChanged = true;
                        }
                    }
                    
                    if(isDelayChanged) 
                    {
                        std::shared_ptr<Road> cpRoad = nullptr;
                        if(!route.isReversed)
                            cpRoad = road->nextRoad;
                        else 
                            cpRoad = road->prevRoad;
                        while(cpRoad != NULL) 
                        {
                            cpRoad->delay += new_delay;
                            lastRouteDelay = cpRoad->delay;
                            if(!route.isReversed)
                                 cpRoad = cpRoad->nextRoad;
                            else 
                                 cpRoad = cpRoad->prevRoad;
                        }
                        delay_add = true;
                    }
                    ok = true;
                }
                if(!route.isReversed)
                    road = road->nextRoad;
                else 
                    road = road->prevRoad;
            }

            prevStartTime = route.startTime;
            prevTotalTime = total_time;
        }

        /// Daca cumva programul trenului activ va trece peste inceputul urmatorului tren, atunci actualizam inceputul urmatorului tren si schimbam delay-ul
        if(delay_add) 
        {        
            if(lastRoute != NULL && startTime + prevTotalTime + lastRouteDelay > lastRoute->startTime) 
            {
                int dif = (startTime + prevTotalTime + lastRouteDelay) - lastRoute->startTime;
                lastRoute->startTime = startTime + prevTotalTime + lastRouteDelay;
                std::shared_ptr<Road> road = lastRoute->road;
                while(road != NULL)
                {
                    road->delay += dif;
                    if(!lastRoute->isReversed)
                         road = road->nextRoad;
                    else 
                         road = road->prevRoad;
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

std::shared_ptr<Road> findRoadById(int road_id)
{
    for(unsigned int i = 0;i < roads.size(); i++)
        if(roads[i]->road_id == road_id)
            return roads[i];
    return NULL;
}

void fetchRoad(XMLElement * element)
{
    std::string road_id = element->Attribute("id");
    std::shared_ptr<Road> road;
    std::shared_ptr<Road> prevRoad = nullptr;
    std::shared_ptr<Road> roadRoot = nullptr;
    XMLElement * segment = element->FirstChildElement();
    while(segment != NULL)
    {
        std::string to_station = segment->Attribute("to_station");
        std::string from_station = segment->Attribute("from_station");
        std::string time = segment->Attribute("time");
        road = std::make_shared<Road>(std::stoi(road_id), 0, std::stoi(time), std::stoi(from_station), std::stoi(to_station));
        if(roadRoot == nullptr) roadRoot = road;
        if(prevRoad != nullptr)
            prevRoad->nextRoad = road, road->prevRoad = prevRoad;
        prevRoad = road;
        segment = segment->NextSiblingElement();
    }
    road->nextRoad = nullptr; roadRoot->prevRoad = nullptr;
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
    std::cout << header << "Client " << client_id << " quitted the applcation and is no longer receiving alarms!\n";
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
        ///std::cout << request << '\n';
        if((bytes_read = read(client, request, sizeof(request))) <= 0) 
        {
            std::cout << header << "Unable to read from client!\n" << '\n';
            continue;
        }
        if(strlen(request) > 0) 
        {
            std::cout << header << "Received " << request << " from client!\n";
            std::string command(request);
            std::pair<bool, std::vector<std::string>> valid = validateCommand(command, header);
            if(!valid.first) 
            {
                char response[BUFFER_SIZE];
                memset(response, 0, sizeof(response)); 
                strcpy(response, "Invalid command\n");
                if(write(client, response, sizeof(response)) <= 0)
                    std::cout << header << "Error: Cannot send response back to the client!\n";
                continue;
            }
            client_id = std::stoi(valid.second.back());
            handleCommand(command, header, valid.second, client);

            if(command.substr(0, 4) == "quit") 
                break;
        }

        if(client_id != -1 && !isThreadActive[client_id]) 
        {
            t = new std::thread(updateNotifications, client, client_id, header);
            isThreadActive[client_id] = 1;
        }
        
    }

    if(client_id != -1)
        isThreadActive[client_id] = 0;
    
    if(t->joinable())
        t->join();
    
    delete t;
}

void notifyClient(int client, int client_id, std::string header)
{
    Database database("users.db");
    std::vector<int> notifiableTrains = database.getNotifiableTrains(client_id);
    int day_id = globalTime.getDayIndex();
    std::string schedule;
    schedule += "Notification:\n";
    for(auto train_id : notifiableTrains)
    {
        for(auto train : trains)
        {
            if(train->train_id == train_id)
            {
                for(auto route : train->schedule[day_id])
                {
                    std::shared_ptr<Road> road = route.road;
                    TimeStamp startTime = route.startTime;
                    int total_time = 0;
                    while(road != NULL)
                    {
                        total_time += road->time;
                        int dif = (startTime + total_time + road->delay) - globalTime;
                        if(dif == 5 || dif == 15 || dif == 30) 
                        {
                            schedule += "Train " + std::to_string(train_id) + " arrives in station " + std::to_string(road->to_station) + " in " + std::to_string(dif) + " minutes. "; 
                            if(road->delay > 0)
                                schedule += "Train is delayed by " + std::to_string(road->delay) +  " minutes\n";
                            else if(road->delay < 0)
                                schedule += "Train arrives earlier by " + std::to_string(-road->delay) + " minutes\n";
                            else 
                                schedule += "All according to the plan. No delays\n";
                        }
                        if(dif > 0)
                            break;
                        road = road->nextRoad;
                    }

                }
            }
        }
    }
    if(schedule.size() > 14) 
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
        {
            std::cout << header + "Too many arguments for command " + commandName << '\n';
            answer.first = false;
        }
    }
    else if(commandName == "add-delay")
    {
        if(args.size() != 3)
            std::cout << header + "Syntax " + commandName + "add-delay <train_id> <number_of_minutes>" << '\n', answer.first = false;
    }
    else if(commandName == "next-hour-departures")
    {
        if(args.size() != 1)
            std::cout << header + "Command " + commandName + " doesn't require any arguments!" << '\n', answer.first = false;
    }
    else if(commandName == "next-hour-arrivals")
    {
        if(args.size() != 1)
            std::cout << header + "Command " + commandName + " doesn't require any arguments!" << '\n', answer.first = false;
    }
    else if(commandName == "register")
    {
        if(args.size() != 3)
            std::cout << header + "Command " + commandName + " syntax: register <username> <password>" << '\n', answer.first = false;
    }
    else if(commandName == "login")
    {
        if(args.size() != 3)
            std::cout << header + "Command " + commandName + " syntax: login <username> <password>" << '\n', answer.first = false;
    }
    else if(commandName == "help")
    {
        if(args.size() > 2)
            std::cout << header << "Too many arguments for command " << commandName << '\n', answer.first = false;
    }
    else if(commandName == "add-alarm")
    {
        if(args.size() != 2)
            std::cout << header << "Command " + commandName + " syntax: add-alarm <train_id>" << '\n', answer.first = false;
    }
    else if(commandName == "logout")
    {
        if(args.size() != 1)
            std::cout << header << "Command " + commandName + " doesn't require any parameters" << '\n', answer.first = false;
    }
    else if(commandName == "quit")
    {
        if(args.size() != 1)
            std::cout << header << "Command " + commandName + " doesn't require any parameters" << '\n', answer.first = false;
    }
    else if(commandName == "disable-alarm")
    {
        if(args.size() != 2)
            std::cout << header << "Comamnd " + commandName + " syntax: disable-alarm <train_id>\n", answer.first = false;
    }
    else if(commandName == "set-location")
    {
        if(args.size() != 2)
            std::cout << header << "Command " + commandName + " syntax: set-station <station_id>\n", answer.first = false;
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
    return "Update successful. Added delay to train " + std::to_string(train_id) + "\n";
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

    std::string scheduleHash = "Schedule ";
    if(train_id != -1)
        scheduleHash += std::to_string(train_id) + " ";
    if(!day.empty())
        scheduleHash += day + " ";
    
    if(scheduleCache.find(scheduleHash) != scheduleCache.end())
    {
        std::string schedule = scheduleCache.find(scheduleHash)->second;
        std::cout << header << "Used cached!\n";
        return schedule;
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
                std::shared_ptr<Road> road = route.road;
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
    scheduleCache[scheduleHash] = schedule;
    return schedule;
}

std::string handle_nextHourArrivals(int client_id)
{
    Database database("users.db");
    int location = database.getLocation(client_id);

    std::string schedule;
    int day_id = globalTime.getDayIndex();
    schedule += "Current Time " + globalTime.to_string() + "\n";
    for(auto train : trains)
    {
        for(auto route : train->schedule[day_id])
        {
             std::shared_ptr<Road> road = route.road;
             int total_time = 0;
             TimeStamp startTime = route.startTime;

             if(route.isReversed) 
             {
                 while(road->nextRoad != NULL)
                    road = road->nextRoad;
             }

             while(road != NULL)
             {
                total_time += road->time;                
                int minutes = (startTime + total_time + road->delay) - globalTime;
                if(minutes >= MIN_ALARM_TIME && minutes <= MAX_ALARM_TIME)
                {   
                    bool isInfoValid = true;
                    if(location > 0)
                    {
                        if(!route.isReversed && road->to_station != location || route.isReversed && road->from_station != location)
                            isInfoValid = false;
                    }
                    if(isInfoValid) 
                    {
                        if(!route.isReversed)
                            schedule += "Train " + std::to_string(train->train_id) + " arrives in station " + std::to_string(road->to_station) + " in " + std::to_string(minutes) + " minutes. ";
                        else 
                            schedule += "Train " + std::to_string(train->train_id) + " arrives in station " + std::to_string(road->from_station) + " in " + std::to_string(minutes) + " minutes. ";

                        if(road->delay == 0)
                            schedule += "All according to the plan. No delays\n";
                        else if(road->delay > 0)
                            schedule += "Train is delayed by " + std::to_string(road->delay) +  " minutes\n";
                        else if(road->delay < 0)
                            schedule += "Train arrives earlier by " + std::to_string(-road->delay) + " minutes\n";
                    }
                }
                if(!route.isReversed)
                    road = road->nextRoad;
                else road = road->prevRoad;
             }
        }
    }
    return schedule;
}

std::string handle_nextHourDepartures(int client_id)
{
    Database database("users.db");
    int location = database.getLocation(client_id);
    std::string schedule;
    int day_id = globalTime.getDayIndex();
    schedule += "Current Time " + globalTime.to_string() + "\n";
    for(auto train : trains)
    {
        for(auto route : train->schedule[day_id])
        {
             std::shared_ptr<Road> road = route.road;
             int total_time = 0;
             TimeStamp startTime = route.startTime;

             if(route.isReversed) 
             {
                 while(road->nextRoad != NULL)
                    road = road->nextRoad;
             }

             while(road != NULL)
             {
                int minutes = (startTime + total_time + road->delay) - globalTime;
                if(minutes >= MIN_ALARM_TIME && minutes <= MAX_ALARM_TIME)
                {   
                    bool isInfoValid = true;
                    if(location > 0)
                    {
                        if(!route.isReversed && road->from_station != location || route.isReversed && road->to_station != location)
                            isInfoValid = false;
                    }
                    if(isInfoValid) 
                    {
                        if(!route.isReversed)             
                            schedule += "Train " + std::to_string(train->train_id) + " leaves station " + std::to_string(road->from_station) + " in " + std::to_string(minutes) + " minutes. ";
                        else
                            schedule += "Train " + std::to_string(train->train_id) + " leaves station " + std::to_string(road->from_station) + " in " + std::to_string(minutes) + " minutes. ";
                        if(road->delay == 0)
                            schedule += "All according to the plan. No delays\n";
                        else if(road->delay > 0)
                            schedule += "Train is delayed by " + std::to_string(road->delay) +  " minutes\n";
                        else if(road->delay < 0)
                            schedule += "Train departs earlier by " + std::to_string(-road->delay) + " minutes!\n";
                    }
                }
                total_time += road->time;                
                if(!route.isReversed)
                    road = road->nextRoad;
                else road = road->prevRoad;
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

std::string handle_addAlarm(int train_id, int client_id)
{
    Database database("users.db");
    std::pair<bool,bool> ok = database.isUserLoggedIn(client_id);
    if(!ok.second)
        return "Error: Log into an account to add an alarm!\n";
    if(!database.isAlarmOnFor(train_id, client_id)) 
    {
        database.insertNotification(train_id, client_id);
        return "Added alarm for train " + std::to_string(train_id) + " succesfully!\n";
    }
    else 
        return "Alarm for train " + std::to_string(train_id) + " already exists!\n";
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

std::string handle_help(std::string commandName)
{
    std::string helpMenu;
    for(auto element : helpMessages)
        if(commandName == "undefined" || element.first == commandName)  
            helpMenu = helpMenu + "\n" + element.first + " - " + element.second + "\n";

    return helpMenu;
}

std::string handle_quit(int client_id)
{
    Database database("users.db");
    bool isClientDeleted = database.deleteClient(client_id);
    if(!isClientDeleted)
        return "Error at deleting client " + std::to_string(client_id) + " from the database\n";
    return "quit\n";
}

std::string handle_disableAlarm(int train_id, int client_id)
{
    Database database("users.db");
    if(!database.isAlarmOnFor(train_id, client_id))
        return "Error: There is no alarm added for train " + std::to_string(train_id) + " by client " + std::to_string(client_id) + "\n";
    else 
    {
        bool isDisabled = database.disableAlarm(train_id, client_id);
        if(!isDisabled)
            return "Error: Not able to disable alarm for train " + std::to_string(train_id) + "\n";
        return "Disabled alarm for train " + std::to_string(train_id) + "\n";
    }
}

std::string handle_setLocation(int station_id, int client_id)
{
    Database database("users.db");
    std::pair<bool, bool> response = database.isUserLoggedIn(client_id);
    if(!response.second)
        return "Error: Login to activate location!\n";
    else 
    { 
        database.setLocation(station_id, client_id);
        return "Set location to " + std::to_string(station_id) + "\n";
    }
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
        res = handle_nextHourArrivals(std::stoi(args[0]));
    else if(command_name == "next-hour-departures")
        res = handle_nextHourDepartures(std::stoi(args[0]));
    else if(command_name == "register")
        res = handle_register(args[0], args[1], std::stoi(args[2]));
    else if(command_name == "login")
        res = handle_login(args[0], args[1], std::stoi(args[2]));
    else if(command_name == "add-alarm")
        res = handle_addAlarm(std::stoi(args[0]), std::stoi(args[1]));
    else if(command_name == "logout")
        res = handle_logout(std::stoi(args[0]));
    else if(command_name == "help") 
    {
        if(args.size() == 2)
            res = handle_help(args[0]);
        else 
            res = handle_help("undefined");
    }
    else if(command_name == "quit")
        res = handle_quit(std::stoi(args[0]));
    else if(command_name == "disable-alarm")
        res = handle_disableAlarm(std::stoi(args[0]), std::stoi(args[1]));
    else if(command_name == "set-location")
        res = handle_setLocation(std::stoi(args[0]), std::stoi(args[1]));
    char response[MAX_SCHEDULE_SIZE];
    memset(response, false, sizeof(response));
    strcpy(response, res.c_str());
    if(write(client, response, sizeof(response)) <= 0)
    {
        std::cout << header << "Can't send to client the response to command " << command_name << '\n';
        return;
    }
}