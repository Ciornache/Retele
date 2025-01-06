#include "timestamp.h"
#include "road.h"
#pragma once

struct ScheduleElement
{
    bool isReversed;
    TimeStamp startTime;
    Road * road;
};

struct Train
{
    int train_id;
    Road * road;
    std::vector<ScheduleElement> schedule[NUMBER_OF_DAYS];
    std::map<std::string, std::vector<int>> delays;
    Train(int train_id, Road * road) : train_id(train_id), road(road) {};
    void addToSchedule(int day_id, bool isReversed, TimeStamp timestamp);
    void addDelay(TimeStamp time, int delay);
};


