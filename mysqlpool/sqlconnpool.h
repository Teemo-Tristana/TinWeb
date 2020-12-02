#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <queue>
#include <mutex>
#include <string>
#include <thread>
#include <assert.h>
#include <semaphore.h>
#include <mysql/mysql.h>

#include "../log/log.h"

// 默认连接数量
const int SQLCONN_NUMBER = 12;

class SqlConnPool{
    public:


        void init(const char* host, int port, const char* user, 
                  const char* pwd, const char* dbName, int connSize);

        static SqlConnPool* instance();

        MYSQL* getConn();
        void freeConn(MYSQL*sql );
        int getFreeConnCount();

        void closePool();
    private:
        SqlConnPool();
        ~SqlConnPool();

    private:
        size_t max_conn;
        size_t use_count;
        size_t free_count;

        std::queue<MYSQL*> connQue;
        std::mutex mtx;
        sem_t semId;


};

#endif