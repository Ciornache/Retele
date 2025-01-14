#include "helpmessages.h"

std::map<std::string, std::string> helpMessages = {
    { 
        "get-schedule", R"(get-schedule <train-id> <day> - Displays the schedule of the trains. Additional parameters can be provided such as train-id or day. 
These parameters filter the output of the get-schedule command. 
train-id specifies for which train to print the schedule. Day specifies the day of the schedule)"
    }, 
    {
        "next-hour-arrivals", R"(Displays all the trains, the station and the time they will arrive in the next hour)"
    }, 
    {
        "next-hour-departures", "Display all the trains, the station and the time they will leave in the next hour"
    }, 
    {
        "add-delay", R"(add-delay <train-id> <time> - Adds a delay of time minutes for the train with id <train-id>. The delay will be added from the current station the train is heading to. 
It will last until the train finishes it's route.)"
    }, 
    {
        "add-alarm", R"(add-alarm <train-id> - Adds an alarm for train <train-id>. The alarm will be triggered every 5, 15 or 30 minutes before a train arrives at it's next station.
You can add an alarm only if you have an account created in the application and you are logged in)"
    }, 
    {
        "disable-alarm", "Disables an alarm if that alarm was previously added."
    }, 
    {
        "login", "login <username> <password> - With a username and password specified, login in the application"
    }, 
    {
        "register", "register <username> <password> - Create an account using the username and password specified as parameters"
    }, 
    {
        "logout", "Exits the account. The user will continue to receive notifications"
    }, 
    {
        "quit", "Exits the application. All the alarms placed will be deleted"
    }, 
    {
        "help", "Prints informations about the application functionalities"
    }, 
    {
        "set-location <station-id>", "Activates location. Location will be <station-id>"
    }
};
