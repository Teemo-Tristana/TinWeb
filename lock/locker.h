/*
 * @Author: hancheng 
 * @Date: 2020-07-10 16:23:10 
 * @Last Modified by:   hancheng 
 * @Last Modified time: 2020-07-13 16:23:10 
 */
#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

//封装信号量的类
class sem
{
private:
    sem_t m_sem;

public:
    //创建并初始化信号量
    sem()
    {
        if (0 != sem_init(&m_sem, 0, 0)) //0 表示局部信号量,信号量的初始值为0
        {
            // perror("sem_init error\n");
            throw std::exception();
        }
    }

    //指定信号量的初始值
    sem(int num)
    {
        if (0 != sem_init(&m_sem, 0, num) != 0) //局部信号量,信号量初始值为 num
        {
            // perror("sem_init error\n");
            throw std::exception();
        }
    }

    ~sem()
    {
        sem_destroy(&m_sem);
    }

    bool wait() // 等待信号量(P操作)
    {
        return sem_wait(&m_sem) == 0;
    }

    bool post() // 增加信号量(V操作)
    {
        return sem_post(&m_sem) == 0;
    }
};

//封装互斥锁的类
class locker
{
private:
    pthread_mutex_t m_mutex;

public:
    locker()
    {
        if (0 != pthread_mutex_init(&m_mutex, NULL))
        {
            // perror("pthread_mutex_init error\n");
            throw std::exception();
        }
    }

    //销毁互斥锁
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    //获取互斥锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    //释放互斥锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    //获取互斥锁
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }
};

//封装条件变量的类
class cond
{
private:
    // pthread_mutex_t m_mutex; //在 locker类中，用get函数可以直接获取锁，因此此时mutex可以被替换掉
    pthread_cond_t m_cond;

public:
    //创建并初始化条件变量
    cond()
    {
        if (0 != pthread_cond_init(&m_cond, NULL))
        {
            // perror("pthread_cond_init error\n");
            throw std::exception();
        }
    }

    //销毁条件变量
    ~cond()
    {
        // pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

    //等待条件变量
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret = 0;
        // pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return 0 == ret;
    }

    //超时等待，如果超时或有信号触发，线程唤醒
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret = 0;
        // pthread_mutex_lock(m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        // pthread_mutex_unlock(m_mutex);
    }

    //唤醒某一个等待目标条件变量的线程
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    // 以广播的方式唤醒所有等待目标条件变量的线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};

#endif
