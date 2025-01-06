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
    public:
        Database(std::string table_name);
        sqlite3 * getConnection();
        int createTable(std::string sqlStatement);
        bool insertUser(std::string name, std::string password, int user_id);
        bool insertNotification(int train_id, int client_id);
        bool insertClient(int client_id, int logged);
        bool validateUser(std::string name, std::string password);
        std::pair<bool,bool> isUserLoggedIn(int user_id);
        bool clearTable(std::string table_name);
        bool updateUser(int client_id, int logged);
        std::vector<int> getNotifiableTrains(int client_id);
        ~Database();
};