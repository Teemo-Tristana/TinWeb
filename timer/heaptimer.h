#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

/**
 * 最小堆
 * 涉及知识点：
 *    deque
 *    vector
 *    unordered_map
 *    堆(数组实现)
 *    C++11新特性 ： std::function、 std::chrono 
 *    回调函数
 *    重载运算符
 *    内联函数 inline
*/

#include <queue>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <chrono>
#include <vector>

#include <time.h>
#include <assert.h>

#include <arpa/inet.h>

#include "../log/log.h"

const int HEAP_DEFAULT_NUM = 64;

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;


struct TimerNode{
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t)
    {
        return expires < t.expires;
    }
};



class HeapTimer{
    public:
        HeapTimer();
        ~HeapTimer();

        void adjust(int id, int newExpires);
        void add(int id, int timeOut, const TimeoutCallBack& cb);

        void doWork(int id);
        void clear();
        void tick();
        void pop();
        int getNextTick();

    private:
        std::vector<TimerNode> v_heap;
        std::unordered_map<int, size_t> record;


        void swapNode(size_t i, size_t j);
        void siftUp(size_t i);
        bool siftDown(size_t index, size_t n);
        void del(size_t i);
   
};

#endif