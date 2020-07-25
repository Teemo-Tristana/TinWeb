/*
 * @Author: hancheng 
 * @Date: 2020-07-11 11:12:02 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 11:24:57
 */

/*******************************************************************\
C++日志模块之阻塞队列:
1.阻塞队列用于异步日志模式
2. 循环数组实现的阻塞队列 m_black = (m_black + 1) % m_max_size
3. 线程安全 每次操作前都要先加互斥锁，操作完成后，再解锁
*******************************************************************/
/**
 * 1.生产者-消费者模型
 * 2.阻塞队列作为缓冲区. 用循环数组实现阻塞队列[producer-consumer共享缓冲区]
 * 3.尝试一个STL中queue
 * >* 
 *      > 队列为空,从队列中获取元素的线程被挂起
 *      > 队列为满,往队列中添加元素的线程被挂起 
*/
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "../lock/locker.h"

using namespace std;

//阻塞式队列, T是模板类
template <class T>
class block_queue
{

private:
    locker m_mutex; //互斥锁
    cond m_cond;    //条件变量

    int m_max_size; //最大长度
    T *m_array;     //队列数组,这里用数组实现队列
    int m_size;     //现有长度
    int m_front;    //队首
    int m_back;     //队尾

public:
    block_queue(int max_size = 1000)
    {
        if (max_size <= 0)
        {
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    void clear()
    {
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    ~block_queue()
    {
        m_mutex.lock();
        if (nullptr != m_array)
        {
            delete[] m_array;
            m_array = nullptr;
        }
        m_mutex.unlock();
    }

    //判断队列是否已满, 为什么不能加const
    bool full()
    {
        m_mutex.lock();
        if (m_size >= m_max_size)
        {

            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //判断队列是否为空
    bool empty()
    {
        m_mutex.lock();
        if (0 == m_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //返回队首元素
    bool front(T &value)
    {
        m_mutex.lock();
        if (0 == m_size)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    //返回队尾元素
    bool back(T &value)
    {
        m_mutex.lock();
        if (0 == m_size)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    //返回队列现有元素个数
    int size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();

        return tmp;
    }

    //返回队列的容量[最多可以有多少个元素]
    int max_size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_max_size;
        m_mutex.unlock();

        return tmp;
    }

    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列,相当于生产者生产了一个元素
    //若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &item) //往队列中添加,相当于生产者
    {

        m_mutex.lock();
        if (m_size >= m_max_size)
        {

            m_cond.broadcast(); //广播唤醒所有等待的线程
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;

        ++m_size;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    //pop时,如果当前队列没有元素,将会等待(阻塞)条件变量[一直阻塞等待]
    bool pop(T &item)//从队列中取出,相当于消费者
    {

        m_mutex.lock();
        while (m_size <= 0)
        {

            if (!m_cond.wait(m_mutex.get())) //一直阻塞直到目标出现
            {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        --m_size;
        m_mutex.unlock();
        return true;
    }

    //重载pop: 增加超时等待(等待超过一定时间后不再等待)
    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};  //s ns
        struct timeval now = {0, 0}; // s us
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timewait(m_mutex.get(), t)) //指定时间超时或目标出现则返回
            {
                m_mutex.unlock();
                return false;
            }
        }

        if (m_size <= 0)
        {
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        --m_size;
        m_mutex.unlock();
        return true;
    }
};

#endif
