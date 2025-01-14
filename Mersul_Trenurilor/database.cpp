#include "database.h"

Database::Database(std::string db_name)
{
    int rc = sqlite3_open(db_name.c_str(), &this->conn);
    if(rc) 
        this->conn = NULL;
}

int Database::createTable(std::string sql_statement)
{
    sqlite3_stmt *stmt = NULL;

    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, nullptr);
    if (err != SQLITE_OK) 
        return 1;

    err = sqlite3_step(stmt);
    if (err != SQLITE_DONE) 
    {  
        sqlite3_finalize(stmt); 
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

bool Database::insertUser(std::string name, std::string password, int user_id)
{
    std::string sql_statement = "SELECT * FROM users WHERE name = ? AND password = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK) 
    {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), name.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), password.size(), SQLITE_STATIC);
    
    if(sqlite3_step(stmt) == SQLITE_ROW) 
    {
        sqlite3_finalize(stmt);
        return false;
    }

    sql_statement = "INSERT INTO users(user_id, name, password) VALUES(?, ?, ?)";
    err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK)
        return false;
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), name.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password.c_str(), password.size(), SQLITE_STATIC);
    
    if(sqlite3_step(stmt) != SQLITE_DONE) 
    {
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::insertClient(int client_id, int logged)
{
    std::string sql_statement = "INSERT INTO clients(client_id, logged, station_id) VALUES(?, ?, ?)";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK) 
        return false;

    sqlite3_bind_int(stmt, 1, client_id);
    sqlite3_bind_int(stmt, 2, logged);
    sqlite3_bind_int(stmt, 3, 0);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::insertNotification(int train_id, int client_id)
{
    std::cout << "Inserting notification for train " << train_id << " in the " << client_id << " account!\n";
    std::string sql_statement = "INSERT INTO alarms(client_id, train_id) VALUES(?, ?)";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK) 
        return false;

    sqlite3_bind_int(stmt, 1, client_id);
    sqlite3_bind_int(stmt, 2, train_id);
    
    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::validateUser(std::string name, std::string password)
{
    std::string sql_statement = "SELECT * FROM users WHERE name = ? AND password = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK) 
    {
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), name.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), password.size(), SQLITE_STATIC);
    
    if(sqlite3_step(stmt) != SQLITE_ROW) 
    {
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::pair<bool, bool> Database::isUserLoggedIn(int user_id)
{
    std::string sql_statement = "SELECT logged FROM clients WHERE client_id = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != 0)
    {
        sqlite3_finalize(stmt);
        return {false, false};
    }
    sqlite3_bind_int(stmt, 1, user_id);
    if(sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return {false, false};
    }

    int isLoggedIn = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return {true, isLoggedIn};

}

bool Database::clearTable(std::string table_name)
{
    std::string sql_statement = "DELETE FROM " + table_name + ";";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK)
    {
        sqlite3_finalize(stmt);
        return false;
    }
    err = sqlite3_step(stmt);
    if(err != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }
    return true;
}

std::vector<int> Database::getNotifiableTrains(int client_id)
{
    std::string sql_statement = "SELECT train_id FROM alarms WHERE client_id = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != 0) 
        return std::vector<int>{};
    std::vector<int> answer;
    sqlite3_bind_int(stmt, 1, client_id);
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        int train_id = sqlite3_column_int(stmt, 0);
        answer.push_back(train_id);
    }
    sqlite3_finalize(stmt);
    return answer;
}

Database::~Database()
{
    sqlite3_close_v2(this->conn);
}

bool Database::updateUser(int client_id, int logged)
{
    std::string sql_statement = "UPDATE clients SET logged = ? WHERE client_id = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK) 
        return false;

    sqlite3_bind_int(stmt, 1, logged);
    sqlite3_bind_int(stmt, 2, client_id);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::deleteClient(int client_id)
{
    std::string sql_statement = "DELETE FROM clients WHERE client_id = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, client_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_reset(stmt);
    sql_statement = "DELETE FROM alarms WHERE client_id = ?";
    err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, client_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    return true;

}

bool Database::isAlarmOnFor(int train_id, int client_id)
{
    std::string sql_statement = "SELECT * FROM alarms WHERE train_id = ? AND client_id = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, train_id);
    sqlite3_bind_int(stmt, 2, client_id);
    if(sqlite3_step(stmt) != SQLITE_ROW) 
    {    
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::disableAlarm(int train_id, int client_id)
{
    std::string sql_statement = "DELETE FROM alarms WHERE train_id = ? AND client_id = ?";
    sqlite3_stmt * stmt;
    int err = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(err != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, train_id);
    sqlite3_bind_int(stmt, 2, client_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

void Database::createTableClients(std::string header)
{
    Database database("users.db");
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS clients (
            client_id INTEGER PRIMARY KEY,
            logged INTEGER NOT NULL, 
            station_id INTEGER NOT NULL
        );
    )";

    int ok = database.createTable(sql);
    if(ok != 0)
        std::cout << header << "Error: Not able to create table logged!\n";
}

void Database::createTableUsers(std::string header)
{
    Database database("users.db");
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            password TEXT NOT NULL
        );
    )";
    int ok = database.createTable(sql);
    if(ok != 0)
        std::cout << header << "Error: Not able to create table users!\n";
}

void Database::createTableAlarms(std::string header)
{
    Database database("users.db");
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS alarms (
            alarm_id INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id INTEGER NOT NULL,
            train_id INTEGER NOT NULL
        );
    )";

    int ok = database.createTable(sql);
    if(ok != 0)
        std::cout << header << "Error: Not able to create table alarms!\n";
}

void Database::initDatabase(std::string header)
{
    Database db("users.db");
    bool ok = db.clearTable("alarms");
    if(!ok) 
        std::cout << header << "Error: Cannot clear table alarms!\n";
    ok = db.clearTable("clients");
    if(!ok)
        std::cout << header << "Error: Cannot clear table clients!\n";

    Database::createTableUsers(header);
    Database::createTableClients(header);
    Database::createTableAlarms(header);
}

bool Database::setLocation(int station_id, int client_id)
{  
    std::string sql_statement = R"(
        UPDATE clients
        SET station_id = ?
        WHERE client_id = ?
    )";
    sqlite3_stmt * stmt;
    int ok = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(ok != SQLITE_OK)
        return false;
    sqlite3_bind_int(stmt, 1, station_id);
    sqlite3_bind_int(stmt, 2, client_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

int Database::getLocation(int client_id)
{
    std::string sql_statement = R"(
        SELECT station_id FROM clients
        WHERE client_id = ?
    )";
    sqlite3_stmt * stmt;
    int ok = sqlite3_prepare_v2(this->conn, sql_statement.c_str(), -1, &stmt, 0);
    if(ok != SQLITE_OK)
        return -1;
    sqlite3_bind_int(stmt, 1, client_id);
    if(sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return -1;
    }
    int location = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return location;
}