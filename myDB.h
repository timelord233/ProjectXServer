#ifndef myDB_class
#define myDB_class
 
#include <iostream>
#include <string>
#include <mysql/mysql.h>
 
class myDB
{
public:
	myDB();
	~myDB();
	int initDB(std::string host, std::string user, std::string password, std::string db_name);
	int select_username(std::string username);
    int check_password(std::string username,std::string password,int& playerid);
	int insertSQL(std::string username,std::string password);
	int deleteSQL();
	int updateSQL();
	MYSQL *connection;
	MYSQL_RES *result;
	MYSQL_ROW row;
};
 
#endif