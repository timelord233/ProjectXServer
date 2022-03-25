#include "myDB.h"
#include <iostream>
#include <cstdlib>
 
using namespace std;
 
myDB::myDB()
{
	connection = mysql_init(NULL); // 初始化数据库的连接变量
	if(connection == NULL)
		{
			std::cout << "error:" << mysql_error(connection);
			exit(1);			// exit(1)表示发生错误后退出程序， exit(0)表示正常退出
		}
}
 
myDB::~myDB()
{
	if(connection != NULL)
		{
			mysql_close(connection); // 关闭数据库连接
		}
}
 
int myDB::initDB(std::string host, std::string user, std::string password, std::string db_name)
{
    char value = 1;
    mysql_options(connection,MYSQL_OPT_RECONNECT,(char*)&value);//mysql自动重连
        
	connection = mysql_real_connect(connection, host.c_str(), user.c_str(), password.c_str(), db_name.c_str(), 0, NULL, 0); // 建立数据库连接
	if(connection == NULL)
	{
		std::cout << "error:" << mysql_error(connection);
		exit(1);			// exit(1)表示发生错误后退出程序， exit(0)表示正常退出
	}
	return 0;
}
 
int myDB::select_username(std::string username)
{
    // mysql_query()执行成功返回0，失败返回非0值。与PHP中不一样
	std::string sql;
	sql = "select username from player where username = '" + username +"';";
    int flag;
    if(mysql_query(connection, sql.c_str()))
    {
		std::cout << "Query Error:" << mysql_error(connection);
        exit(1);
    }
    else
    {
        result = mysql_store_result(connection); // 获取结果集
        if (result->row_count > 0) flag=1;
        else flag=0;
        mysql_free_result(result);
    }
    return flag;
}
 
int myDB::check_password(std::string username,std::string password,int& playerid){
    std::string sql;
	sql = "select * from player where username = '" + username +"';";
    int flag;
    if(mysql_query(connection, sql.c_str()))
    {
		std::cout << "Query Error:" << mysql_error(connection);
        exit(1);
    }
    else
    {
        result = mysql_store_result(connection); // 获取结果集
        row = mysql_fetch_row(result);
		
		if (result->row_count > 0) {
			if (row[2] == password) {
				flag=1;
				playerid = atoi(row[0]);
			}
        	else flag=0;
		}
		else flag=0;
        //printf("%s %s %s\n",row[0],row[1],row[2]);
        mysql_free_result(result);
    }
    return flag;
}
int myDB::insertSQL(std::string username,std::string password)
{

	string sql = "insert into player(username,password) values ( '" + username +"','"+ password +"');";
	if(mysql_query(connection, sql.c_str()))
	{
		std::cout << "Query Error:" << mysql_error(connection);
		exit(1);
	}
	return 0;
}
 
int myDB::deleteSQL()
{
	return 0;
}
 
int myDB::updateSQL()
{
	
	return 0;
	
}
 
