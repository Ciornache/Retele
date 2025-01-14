#include <sqlite3.h>
#include <string>
#include <cstring>
#include <iostream>
#include <vector>
#include <utility>

#define QUERY_SIZE 1024

class Database
{
    private:
        sqlite3 * conn;
        static void createTableUsers(std::string header);
        static void createTableAlarms(std::string header);
        static void createTableClients(std::string header);
        bool clearTable(std::string table_name);

    public:

        /* Init Operations */

        Database(std::string db_name);
        ~Database();
        static void initDatabase(std::string header);
        sqlite3 * getConnection();

        /* Insert Operations */

        bool insertUser(std::string name, std::string password, int user_id);
        bool insertNotification(int train_id, int client_id);
        bool insertClient(int client_id, int logged);

        /* Update Operations */

        bool setLocation(int station_id, int train_id);
        int getLocation(int client_id);
        bool updateUser(int client_id, int logged);
        bool disableAlarm(int train_id, int client_id);

        /* Update/Create Operations */

        bool deleteClient(int client_id);
        int createTable(std::string sqlStatement);
        
        /* Retrieve Operations */

        bool validateUser(std::string name, std::string password);
        std::pair<bool,bool> isUserLoggedIn(int user_id);
        std::vector<int> getNotifiableTrains(int client_id);
        bool isAlarmOnFor(int train_id, int client_id);
};