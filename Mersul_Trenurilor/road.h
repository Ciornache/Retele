#include "timestamp.h"
#include <iostream>
#include <memory>

#pragma once

struct Road : public std::enable_shared_from_this<Road>
{
    int road_id;
    int time;
    int from_station, to_station;
    std::shared_ptr<Road> nextRoad, prevRoad;
    int delay, last_delay = 0;
    Road(int road_id, int delay, int time, int from_station, int to_station);
    Road() {};
    void setNextRoad(std::shared_ptr<Road> road);
    void copyRoad(std::shared_ptr<Road> road);
    std::shared_ptr<Road> clone();
};