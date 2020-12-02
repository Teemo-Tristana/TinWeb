#include "sqlconnpool.h"

using namespace std;

inline SqlConnPool::SqlConnPool():use_count(0), free_count(0){}

inline SqlConnPool::~SqlConnPool(){
    closePool();
}

void SqlConnPool::init(const char* host, int port, const char* user, 
                  const char* pwd, const char* dbName, int connSize=SQLCONN_NUMBER)
{
    assert(connSize > 0);
    for(int i = 0; i < connSize; ++i)
    {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql)
        {
            LOG_ERROR("MySql init error : %s", mysql_error(sql));
            assert(sql);
            exit(1);
        }

        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if (sql == nullptr)
        {
            LOG_ERROR("MySql connect error : %s", mysql_error(sql));
            exit(1);
        }
        connQue.push(sql);
    }

    max_conn = connSize;
    sem_init(&semId, 0, max_conn);

}

SqlConnPool* SqlConnPool::instance()
{
    static SqlConnPool connpool;
    return &connpool;
}

MYSQL* SqlConnPool::getConn()
{
    MYSQL* sql = nullptr;
    if (connQue.empty())
    {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }

    sem_wait(&semId);

    {
        lock_guard<std::mutex> locker(mtx);
        sql = connQue.front();
        connQue.pop();
    }

    return sql;
}

void SqlConnPool::freeConn(MYSQL* sql)
{   
    assert(sql != nullptr);
    lock_guard<std::mutex> locker(mtx);
    connQue.push(sql);
    sem_post(&semId);
}

int SqlConnPool::getFreeConnCount()
{
    lock_guard<std::mutex> locker(mtx);
    return connQue.size();
}

void SqlConnPool::closePool()
{
    lock_guard<std::mutex> locker(mtx);
    while (!connQue.empty())
    {
        auto item = connQue.front();
        connQue.pop();
        mysql_close(item);
    }

    mysql_library_end();
}