/*
 * @Author: hancheng 
 * @Date: 2020-07-11 15:43:18 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 16:23:48
 */

/*
参考实现图8-10的半同步/半反应堆模式:
    工作队列 + 进程池
*/

#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "../lock/locker.h"
#include "../connectionPool/sql_connection_pool.h"

using namespace std;
/*线程池类,定义为模板类便于代码复用 模板参数T是任务类*/

template <typename T>
class threadpool
{
private:
    int m_thread_number; //线程数量
    bool m_stop;         //是否结束线程

    pthread_t *m_threads;  //进程池数组起始地址,数组大小为 m_thread_number
    list<T *> m_workqueue; //请求队列.我们这里用list来存储队列
    int m_max_requests;    //请求队列中最大值

    locker m_lock;               //互斥锁(申请队列时竞争的资源)
    sem m_sem;                   //信号量 通知是否有任务需要处理
    connection_pool *m_connPool; //数据库连接池
public:
    // thread_number 线程池中线程的数量 max_requests 请求队列中最多允许等待处理的请求数量
    threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 1000);
    ~threadpool();
    bool append(T *request); //往队列中添加任务

private:
    //工作线程运行函数,不断从工作队列中取任务并执行
    static void *worker(void *arg);
    void run();
};

//进程池构造函数
template <typename T>
threadpool<T>::threadpool(connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_threads(nullptr), m_stop(false), m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    //创建 thread_number个线程,并将他们设置为脱离线程
    for (int i = 0; i < thread_number; ++i)
    {
        // printf("create the %dth thread\n", i);
        /*1.worker是类的静态成员函数,不能使用一个类的成员,因而将this指针作为参数传递给它,这样就可以使用了类的成员
        */
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        /*设置为脱离线程后,在线程结束时便可以自动释放所有资源,不用再单独回收*/
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

//进程池析构函数
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

// 通过list容器创建请求队列,先申请互斥锁(保证线程安全),再向队列添加,最后通过信号量通知有任务待处理并释放锁
template <typename T>
bool threadpool<T>::append(T *request)
{
    m_lock.lock(); //队列被所有线程共 享,属于临界资源,操作时必须加锁
    if (m_workqueue.size() > m_max_requests)
    {
        m_lock.unlock();
        return false;
    }
    m_workqueue.push_back(request); //添加任务到队列
    m_lock.unlock();
    m_sem.post(); //利用信号量通知有新任务待处理
    return true;
}

/**
 *  线程处理函数(因为使用类的静态函数作为工作函数,而又需要需用到类的成员,
 *  因此,传入的参数是this)这里的arg就是this 
*/
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

//工作线程同请求队列中取任务并处理， 使用信号类俩同步
template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_sem.wait(); //等待信号量(信号量-1)

        //加锁保证线程安全
        m_lock.lock();
        if (m_workqueue.empty())
        {
            m_lock.unlock();
            continue;
        }

        //从任务队列(std::list)取出链任务(链表第一个节点),然后将任务从链表删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();

        m_lock.unlock();
        if (!request)
            continue;

        //从连接池取一个数据库连接
        connectionRAII mysqlcon(&request->mysql, m_connPool);

        //处理任务(模板类的方法,这里是http类)进行处理
        request->process();
    }
}
#endif
