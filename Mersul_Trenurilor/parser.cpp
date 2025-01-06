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
#include <tinyxml2.h>

#include "train.h"

using namespace tinyxml2;

std::vector<Road*> roads;

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
            if(type == "reversed") isReversed = true;
            trainObj->addToSchedule(day_id, isReversed, TimeStamp(departure));
            direction = direction->NextSiblingElement();
        }
        day_schedule = day_schedule->NextSiblingElement();
    }
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

int main(int arg, char * argv[])
{
    fetchData("/home/ciornei/Mersul_Trenurilor/schedule.xml", "Random header");
}