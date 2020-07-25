/*
 * @Author: hancheng 
 * @Date: 2020-07-13 09:37:55 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 10:25:58
 */

/*

连接池模块:
	>* 单例模式,保证唯一
	>* list实现(保存)连接池:
		>* 线程需要时,从list申请
		>* 互斥锁(mutex)保证list中连接资源的安全
		>* 信号量(reserve) 用于管理连接池空连接数
		>* 获取/归还连接时,先使用互斥锁对连接池加锁,再用信号量保证list空闲连接数
		


CGI & 数据库连接池
====================
>* 数据库连接池：
    >* 单例模式，保证唯一
    >* list实现连接池
    >* 连接池为静态大小
    >* 互斥锁实现线程安全


>* CGI
    >* HTTP 请求采取POST方式
    >* 登陆用户名和密码校验
    >* 登陆注册及多线程池注册安全



*/
#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>

#include "../lock/locker.h"

using namespace std;

//连接池类
class connection_pool
{
private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	locker lock;
	list<MYSQL *> connList; //连接池
	sem reserve;

private:
	string url;			 //主机地址
	string Port;		 //数据库端口号
	string User;		 //登陆数据库用户名
	string PassWord;	 //登陆数据库密码
	string DatabaseName; //使用数据库名
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn() const;					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static connection_pool *GetInstance();

	//真正的初始化函数
	void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);

private:
	connection_pool();
	~connection_pool();
};

//连接初始化类:类似C++11的思想,连接即初始化
//将数据库的连接与释放通过RAII机制封装, 避免手动释放
class connectionRAII
{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();

private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
