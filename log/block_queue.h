#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>
#include <assert.h>


/**
 * 涉及知识点：
 *    deque
 *    锁：unique_lock、 lock_guard
 *    条件变量： condtion_varable
 *    时钟: std::chrono
 *    生产者与消费者模型
 *    explicit 关键字
 *    模板类
 *    内联函数 inline
 *    
*/

// 阻塞队列类型
template<class T>
class BlockQueue{
    private: 
        std::deque<T> deq;
        size_t capacity;
        std::mutex mtx;
        bool isClose;
        std::condition_variable consumer_cond;
        std::condition_variable producer_cond;

    public:
        explicit BlockQueue(size_t Max_capacity=1000);
        ~BlockQueue();
        void clear();
        bool empty();
        bool full();
        void close();
        size_t size();
        size_t capacity();

        T front();
        T back();

        void push_back(const T& item);
        void push_front(const T& item);

        bool pop(T&item);
        bool pop(T&item, int timeout);
        void flush();

};

#endif 