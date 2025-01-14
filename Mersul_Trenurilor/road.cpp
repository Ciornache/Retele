#include "road.h"

void Road::setNextRoad(std::shared_ptr<Road> road)
{
    this->nextRoad = road; 
}

void Road::copyRoad(std::shared_ptr<Road> road)
{
    this->road_id = road->road_id;
    this->delay = road->delay;
    this->time = road->time;
    this->from_station = road->from_station;
    this->to_station = road->to_station;
    this->last_delay = road->last_delay;
} 

std::shared_ptr<Road> Road::clone()
{
    std::shared_ptr<Road> root = std::make_shared<Road>(), prevNode = nullptr, copyRoot;
    std::shared_ptr<Road> curr = shared_from_this();
    copyRoot = root;
    while(curr != NULL)
    {
        root->copyRoad(curr);    
        if(prevNode != NULL) prevNode->nextRoad = root;
        root->prevRoad = prevNode; root->nextRoad = NULL;
        if(curr->nextRoad != NULL)
             root->nextRoad = std::make_shared<Road>();
        prevNode = root;
        root = root->nextRoad;
        curr = curr->nextRoad;
    }
    root = NULL;
    return copyRoot;
} 

Road::Road(int road_id, int delay, int time, int from_station, int to_station) : road_id(road_id), delay(delay), time(time), from_station(from_station), to_station(to_station) { this->last_delay = 0; };