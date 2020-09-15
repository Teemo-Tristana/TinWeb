/*
 * @Author: hancheng 
 * @Date: 2020-07-13 09:45:22 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 10:22:55
 */

#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>

#include "sql_connection_pool.h"
#include "../log/log.h"

using namespace std;

//构造函数(初始化)
connection_pool::connection_pool()
{
	this->CurConn = 0;
	this->FreeConn = 0;
	this->MaxConn = 0;
}

//析构函数(RAII机制销毁连接池)
connection_pool::~connection_pool()
{
	DestroyPool();
}

//单例模式中的懒汉模式
connection_pool *connection_pool::GetInstance()
{
	//局部静态变量,线程安全
	//	-> 原因:C++11标准规定:当一个线程正在初始化一个变量时,其他线程必须得等到该初始化完成后才能访问
	static connection_pool connPool;
	return &connPool;
}

//真正的初始化函数
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn)
{
	//初始化数据库信息
	this->url = url;
	this->Port = Port;
	this->User = User;
	this->PassWord = PassWord;
	this->DatabaseName = DBName;

	lock.lock(); //先加锁,再创建MaxConn数据库连接
	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = nullptr; //新创建一个连接资源
		con = mysql_init(con);

		if (nullptr == con)
		{
			cout << "Mysqlinit Error:" << mysql_error(con);
			LOG_ERROR("Mysqlinit Error: %s", mysql_error(con));
			exit(1);
		}

		//连接数据库
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (NULL == con)
		{
			cout << "Mysql Connect Error: " << mysql_error(con);
			LOG_ERROR("Mysql Connect Error: %s", mysql_error(con));
			exit(1);
		}

		//更新连接池和空闲连接数量
		connList.push_back(con);
		++FreeConn;
	}

	//将信号量初始化为最大连接次书
	reserve = sem(FreeConn);
	//重置连接池的最大连接数
	this->MaxConn = FreeConn;

	lock.unlock();
}

/*申请一个连接资源:当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数*/
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = nullptr;

	if (0 == connList.size())
		return nullptr;

	reserve.wait(); //若信号量大于0,则信号量-1, 否则阻塞直到信号量可用

	lock.lock();

	con = connList.front(); //从链表头获取一个资源
	connList.pop_front();

	//这两个成员变量没有用到
	--FreeConn;
	++CurConn;

	lock.unlock();
	return con;
}

//释放当前使用的连接conn(放入连接池)
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (nullptr == con)
		return false;

	lock.lock();

	connList.push_back(con);//放入连接池
	++FreeConn;
	--CurConn;

	lock.unlock();

	reserve.post(); //信号量+1
	return true;
}

//销毁连接池
void connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		// list<MYSQL *>::iterator it;
		for (auto it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con); //获取每一个连接然后关闭
		}
		CurConn = 0;
		FreeConn = 0;
		connList.clear();

		lock.unlock();
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn() const
{
	return this->FreeConn;
}

/**********************************************************/
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
	*SQL = connPool->GetConnection();

	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
	poolRAII->ReleaseConnection(conRAII);
}