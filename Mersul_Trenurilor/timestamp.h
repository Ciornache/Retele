#include <string>
#include <vector>
#include <sstream>
#include <map>
#pragma once

#define NUMBER_OF_DAYS 7


class TimeStamp
{
    int hours;
    int minutes;
    int day_index;
    
    public:

        TimeStamp(int hours, int minutes, int day_index);
        TimeStamp();
        void setHours(int hours);
        void setMinutes(int minutes);
        TimeStamp operator++(int x);
        TimeStamp(std::string timeStamp);
        TimeStamp operator + (int time);
        int operator - (TimeStamp timestamp);
        bool operator > (TimeStamp timestamp);
        bool operator < (const TimeStamp timestamp) const;
        bool operator == (TimeStamp timestamp);
        static int getDayIndex(std::string day);
        int getDayIndex();
        std::string to_string();

};