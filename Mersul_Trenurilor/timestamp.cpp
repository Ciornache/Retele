#include "timestamp.h"
#include <iostream>

TimeStamp::TimeStamp(int hours, int minutes, int day_index = 0)
{
    this->hours = hours;
    this->minutes = minutes;
    this->day_index = day_index;
}

TimeStamp::TimeStamp(std::string timeStamp)
{
    std::stringstream ss;
    std::string hours, minutes;
    ss << timeStamp;
    std::getline(ss, hours, ':');
    std::getline(ss, minutes, ':');
    this->hours = std::stoi(hours);
    this->minutes = std::stoi(minutes);
}

void TimeStamp::setHours(int hours)
{
    this->hours = hours;
}

void TimeStamp::setMinutes(int minutes)
{
    this->minutes = minutes;
}

TimeStamp TimeStamp::operator++(int value)
{
    int newMinutes = (this->minutes + 1) % 60;
    int newHours = (this->hours + (newMinutes == 0)) % 60;
    this->setHours(newHours); this->setMinutes(newMinutes);
    if(newHours == 0 && newMinutes == 0)
        this->day_index++;

    return *this;   
}

int TimeStamp::getDayIndex(std::string day)
{
    const std::vector<std::string> days = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    for(unsigned int i = 0;i < days.size(); i++)
        if(days[i] == day)
            return i;
    return -1;
}

TimeStamp::TimeStamp()
{
    this->hours = 0;
    this->minutes = 0;
}

int TimeStamp::getDayIndex()
{
    return this->day_index;
}

TimeStamp TimeStamp::operator+(int time) 
{
    TimeStamp newTimeStamp;
    int hours(this->hours), minutes(this->minutes);
    hours += time / 60;
    minutes += time % 60;
    if(minutes > 59)
        minutes -= 60, hours++;
    if(hours > 23)
        hours -= 24;
    newTimeStamp.setHours(hours); newTimeStamp.setMinutes(minutes);
    return newTimeStamp;
}

bool TimeStamp::operator>(TimeStamp timestamp)
{
    if(this->hours * 60 + this->minutes > timestamp.hours * 60 + timestamp.minutes)
        return true;
    return false;
}

bool TimeStamp::operator <(const TimeStamp timestamp) const 
{
    if(this->hours * 60 + this->minutes > timestamp.hours * 60 + timestamp.minutes)
        return false;
    return true;
}

bool TimeStamp::operator==(TimeStamp timestamp)
{
    return this->hours == timestamp.hours && this->minutes == timestamp.minutes;
}

std::string TimeStamp::to_string()
{
    std::string timeString;
    if(this->hours < 10) timeString += "0";
    timeString += std::to_string(this->hours) + ":";
    if(this->minutes < 10) timeString += "0";
    timeString += std::to_string(this->minutes);
    return timeString;
}

int TimeStamp::operator-(TimeStamp timestamp)
{
    return (this->hours - timestamp.hours) * 60 + (this->minutes - timestamp.minutes);
}