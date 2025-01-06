#include "train.h"

void Train::addToSchedule(int day_id, bool isReversed, TimeStamp timestamp)
{
    schedule[day_id].push_back({isReversed, timestamp, road->clone()});   
}

void Train::addDelay(TimeStamp time, int delay)
{
    delays[time.to_string()].push_back(delay);
}
