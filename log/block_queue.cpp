
#include "block_queue.h"


template<class T>
inline BlockQueue<T>::BlockQueue(size_t Max_capacity) : capacity(Max_capacity)
{
    assert(Max_capacity > 0);
    isClose = false;
}

template<class T>
inline BlockQueue<T>::~BlockQueue()
{
    close();
}

template<class T>
void inline BlockQueue<T>::clear()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    deq.clear();

}

template<class T>
bool inline BlockQueue<T>::empty()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    return deq.empty();
}

template<class T>
bool inline BlockQueue<T>::full()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    return capacity == deq.size();
}


template<class T>
void inline BlockQueue<T>::close()
{
    {
        std::lock_guard<std::mutex> g_locker(mtx);
        deq.clear();
        isClose = true;
    }
    consumer_cond.notify_all();
    producer_cond.notify_all();
}

template<class T>
size_t inline BlockQueue<T>::size()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    return deq.size();
}

template<class T>
size_t inline BlockQueue<T>::capacity()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    return capacity;
}



template<class T>
T inline BlockQueue<T>::front()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    return deq.front();
}

template<class T>
T inline BlockQueue<T>::back()
{
    std::lock_guard<std::mutex> g_locker(mtx);
    return deq.back();
}

template<class T>
void inline BlockQueue<T>::push_front(const T& item)
{
    std::lock_guard<std::mutex> g_locker(mtx);
    while (deq.size() >= capacity)
    {
        producer_cond.wait(g_locker);
    }
    deq.push_front(item);
    consumer_cond.notify_one();
}

template<class T>
void inline BlockQueue<T>::push_back(const T& item)
{
    std::lock_guard<std::mutex> g_locker(mtx);
    while (deq.size()>= capacity)
    {
        producer_cond.wait(g_locker);
    }
    deq.push_back(item);
    consumer_cond.notify_one();
}

template<class T>
bool inline BlockQueue<T>::pop(T& item)
{
    std::unique_lock<std::mutex> u_locker(mtx);
    while (deq.empty())
    {
        consumer_cond.wait(u_locker);
        if (isClose)
            return false;
        
    }
    item = deq.front();
    deq.pop_front();
    consumer_cond.notify_one();
    return true;
}


template<class T>
bool inline BlockQueue<T>::pop(T& item, int timeout)
{
    std::unique_lock<std::mutex> u_locker(mtx);
    
    while (deq.empty())
    {
        if (std::cv_status::timeout == consumer_cond.wait_for(u_locker, std::chrono::seconds(timeout)))
            return false;
        
        if (isClose)
            return false;
    }

    item = deq.front();
    deq.pop_front();
    consumer_cond.notify_one();
    return true;
}


template<class T>
void inline BlockQueue<T>::flush()
{
    consumer_cond.notify_one();
}