#ifndef THREAD_POOL_H
#define THREAD_POOL_H



#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>

#include <assert.h>


/**
 * 线程池
 * 涉及知识点：
 *    deque
 *    C++11的新特性：
 *      lambda、forward、move、lock_guard、线程分离(detach)
 *      智能指针、lambad表达式、condition_variable 、function相关
 *    explicit关键字、显示构造
 *    默认构造、默认移动构造、
 *    线程池中默认线程的选取问题(如何计算)
 *    
*/

// 线程池数量
static const int THREAD_NUMBER = 8;

class ThreadPool{

    public:
        explicit  ThreadPool(size_t threadCount = THREAD_NUMBER):t_pool(std::make_shared<Pool>())
    {
        assert(threadCount > 0);

        for(size_t i = 0;i  < threadCount; ++i)
        {
            // c++ lambda
            std::thread([pool = t_pool]{
                std::unique_lock<std::mutex> locker(pool->mtx);
                while (true)
                {
                    if (!pool->tasks.empty())
                    {
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        task();;
                        locker.lock();
                    }
                    else if (pool->isclosed)
                        break;
                    else 
                        pool->cond.wait(locker);
                }
            }).detach();
        }
    }

        ThreadPool() = default;

        ThreadPool(ThreadPool&&) = default;

        ~ThreadPool()
    {
        if (static_cast<bool>(t_pool))
        {
            std::lock_guard<std::mutex> locker(t_pool->mtx);
            t_pool->isclosed = true;
        }
        t_pool->cond.notify_all();
    }

        template<class T>
        void addTask(T&& task)
        {
            {
                std::lock_guard<std::mutex> locker(t_pool->mtx);
                t_pool->tasks.emplace(std::forward<T>(task));
            }

        t_pool->cond.notify_one();
        }

    private:
        struct Pool
        {
            std::mutex mtx;
            std::condition_variable cond;
            bool isclosed ;
            std::queue<std::function<void()>> tasks;
        };
        std::shared_ptr<Pool> t_pool;


        

};


#endif