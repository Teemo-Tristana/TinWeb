#include "heaptimer.h"

/* 预分配指定的空间(reserve)[注意 vector 的 reserve() 与 resize() 的区别] */
inline HeapTimer::HeapTimer()
{
    v_heap.reserve(HEAP_DEFAULT_NUM);
}


inline HeapTimer::~HeapTimer()
{
    clear();
}

// 调整指定id的节点
void HeapTimer::adjust(int id, int newExpires)
{
    assert(!v_heap.empty() && record.count(id) > 0);
    v_heap[record[id]].expires = Clock::now()+ MS(newExpires);
    siftDown(record[id], v_heap.size());
}

// 修改指定id的节点
void HeapTimer::add(int id, int expires, const TimeoutCallBack &cb)
{
    assert(id >= 0);
    size_t i = 0;
    // 新节点，堆尾插入，调整堆
    if (record.count(id)  == 0)
    {
        i = v_heap.size();
        record[id] = i;
        v_heap.push_back({id,Clock::now() + MS(expires), cb});
        siftUp(i);
        return;
    }

    // 已有节点，则调整堆
    i = record[id];
    v_heap[i].expires = Clock::now() + MS(expires);
    v_heap[i].cb = cb;
    if (!siftDown(i, v_heap.size()))
        siftUp(i);
}


// 工作函数：删除指定id的节点，并触发回调函数
void HeapTimer::doWork(int id)
{
    if (v_heap.empty() || record[id] == 0)
        return;
    size_t i = record[id];
    TimerNode node = v_heap[i];
    node.cb();
    del(i);
}

// 清空堆和record表
void HeapTimer::clear()
{
    record.clear();
    v_heap.clear();
}

// tick函数：清楚超时的节点
void HeapTimer::tick()
{
    if(v_heap.empty())
        return;
    
    while (!v_heap.empty())
    {
        TimerNode node = v_heap.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)
        {
            break;
        }
        node.cb();
        pop();
    }
}

// 弹出
void HeapTimer::pop()
{
    assert(!v_heap.empty());
    del(0);
}

// 获取下一次tick的时间(ms)
int HeapTimer::getNextTick()
{
    tick();
    size_t res = -1;
    if (v_heap.empty())
    {
        res = std::chrono::duration_cast<MS>(v_heap.front().expires - Clock::now()).count();
        if (res < 0)
            res = 0;
    }
    return res;
}


// 交换两个节点
void HeapTimer::swapNode(size_t i, size_t j)
{
    assert(i >= 0 && i < v_heap.size());
    assert(j >= 0 && j < v_heap.size());
    std::swap(v_heap[i], v_heap[j]);
    record[v_heap[i].id] = i;
    record[v_heap[j].id] = j;
}

// 上调
void HeapTimer::siftUp(size_t i)
{
    assert(i >= 0 && i < v_heap.size());
    size_t j = (i - 1) / 2;
    while (j >= 0)
    {
        if (v_heap[j] < v_heap[i])
            break;
        swapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 下调
bool HeapTimer::siftDown(size_t index, size_t n)
{
    assert(index >= 0 && index < v_heap.size());
    assert(n >= 0 && n <= v_heap.size());

    size_t i = index;
    size_t j = i * 2 + 1;

    while (j < n)
    {
        if (j + 1 < n && v_heap[j+1] < v_heap[j])
            ++j;
        if (v_heap[i] < v_heap[j])
            break;

        swapNode(i, j);
        i = j;
        j = j * 2 + 1;
    }
    return i > index;
}

// 删除指定 id 的节点，并调整堆
void HeapTimer::del(size_t i)
{
    assert(!v_heap.size() > 0 && i >= 0 && i < v_heap.size());
    size_t n = v_heap.size() - 1;

    if (i < n)
    {
        swapNode(i, n);
        if (!siftDown(i, n))
            siftUp(i);
    }

    record.erase(v_heap.back().id);
    v_heap.pop_back();
}
