/*
 * @Author: hancheng 
 * @Date: 2020-07-25 17:41:27 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-25 20:40:33
 */

#include <mysql/mysql.h>
#include <string>
#include <iostream>
#include <string.h>
using namespace std;

//连接数据库
MYSQL *ConnectionDataBase(string url = "127.0.0.1", string User = "root", string PassWord = "root", string DBName = "testdb", int Port = 3306)
{   
    MYSQL *conn = nullptr;
    conn = mysql_init(conn);

    conn = mysql_real_connect(conn, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

    if (!conn)
    {
        cout << "Mysql Connect Error :  " << mysql_error(conn) << endl;
        return nullptr;
    }
    return conn;
}

//建库
bool createdatabase(MYSQL *conn, string databasename = "tinywebdb")
{
    string sql = "create database if not exists " + databasename+";";
    // char *sql = (char *)malloc(sizeof(char) * 255);
    // memset(sql, '\0', 255);
    // sprintf(sql, "create database if not exists %s;", databasename.c_str());
    int res = mysql_query(conn, sql.c_str());

    if (res)
    {
        cout << "creat database" + databasename + "error " << endl;
        return false;
    }
    cout << "create database " << databasename << " ok" << endl;
    return true;
}

//建表
bool createTable(MYSQL *conn, string tablename = "userinfo")
{
    char *sql = (char *)malloc(sizeof(char) * 255);
    memset(sql, '\0', 200);
    sprintf(sql, "create table if not exists `%s`(username char(50) NULL,passwd char(50) NULL)ENGINE=InnoDB;", tablename.c_str());
    int res = mysql_query(conn, sql);
    if (res)
    {
        cout << "creat table error " << endl;
        return false;
    }
    cout << "create table " << tablename << " ok" << endl;
    return true;
}

//关闭连接
void closeconnection(MYSQL *conn)
{
    mysql_close(conn);
}

void create(string url,string user ,string passWord ,int port = 3306,
string dbname = "tinywebdb", string tbname = "userinfo")
{


    //建库
    MYSQL *conn = ConnectionDataBase(url, user, passWord, "", port);
    createdatabase(conn, dbname);
    closeconnection(conn);

    //建表
    conn = ConnectionDataBase(url, user, passWord, dbname, port);
    createTable(conn, tbname);

    closeconnection(conn);
}