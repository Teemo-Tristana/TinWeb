#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <vector>
#include <list>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
using namespace std;

/*
思路： 

*/
template <typename T>
class threadpool
{
private:
    int m_thread_nums;
    int m_max_task_nums;
    vector<pthread_t> m_threads;
    list<T *> tasks;
    locker m_lock;
    sem m_sem;
    bool isRun;
    connection_pool *m_connpool;

public:
    threadpool(connection_pool *connpool, int thread_nums = 8, int max_task_nums = 10000);
    ~threadpool();

    bool append(T *task);

private:
    static void *worker(void *arg);
    void run();
};

template <typename T>
threadpool<T>::threadpool(connection_pool *connpool, int thread_nums, int max_task_nums) : m_connpool(connpool), m_thread_nums(thread_nums), m_max_task_nums(max_task_nums), isRun(false)
{
    if (thread_nums <= 0 || max_task_nums <= 0)
    {
        throw exception();
    }
    m_threads.resize(thread_nums);
    for (int i = 0; i < thread_nums; ++i)
    {
        printf("开始创建线程 %d\n", i + 1);
        if (pthread_create(&m_threads[i], NULL, worker, this) != 0)
        {
            throw exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            throw ::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    isRun = true;
}

template <typename T>
bool threadpool<T>::append(T *task)
{
    m_lock.lock();
    if (tasks.size() > m_max_task_nums)
    {
        m_lock.unlock();
        return false;
    }
    tasks.push_back(task);
    m_lock.unlock();
    m_sem.post();
    return true;
}

template <typename T>
void threadpool<T>::run()
{
    while (!isRun)
    {
        m_sem.wait();

        m_lock.lock();
        if (tasks.empty())
        {
            m_lock.unlock();
            continue;
        }

        T *task = tasks.front();
        tasks.pop_front();
        m_lock.unlock();
        if (!task)
            continue;

        connectionRAII mysqlcon(&task->mysql, m_connpool);

        task->process();
    }
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *thpool = static_cast<threadpool *>(arg);
    thpool->run();
    return thpool;
}
#endif