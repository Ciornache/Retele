#include "timestamp.h"
#include <iostream>
#pragma once

struct Road
{
    int road_id;
    int time;
    int from_station, to_station;
    Road * nextRoad, *prevRoad;
    int delay, last_delay = 0;
    Road(int road_id, int delay, int time, int from_station, int to_station);
    Road() {};
    void setNextRoad(Road * road);
    void copyRoad(Road * road);
    Road * clone();
};