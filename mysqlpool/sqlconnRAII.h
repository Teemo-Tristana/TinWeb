#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H

#include "sqlconnpool.h"


/**
 * RAII : 资源获取即初始化
 * 
 * 资源在对象构造时即初始化，在析构时被释放
 * 
 * 知识点：
 *      RAII 思想用法
 *      比如 智能指针就用到栏 RAII
*/
class sqlConnRAII
{
    public:
        sqlConnRAII(MYSQL** arg_sql, SqlConnPool* arg_connpool)
        {
            assert(arg_connpool != nullptr);
            *arg_sql = arg_connpool->getConn();
            sql = *arg_sql;
            connpool = arg_connpool;
        }

        ~sqlConnRAII()
        {
            if (sql != nullptr)
            {
                connpool->freeConn(sql);
            }
        }
    private:
        MYSQL* sql;
        SqlConnPool* connpool;
};



#endif