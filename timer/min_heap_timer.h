/*
 * @Author: hancheng 
 * @Date: 2020-07-12 19:24:10 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-12 22:38:28
 */
/*
时间堆: 用最小堆实现的定时器
>  最小堆使用数组表示
>  堆顶元素是超时时间(expire)最小的定时器
>  复杂度:
    > 添加add_timer O(lg[n])
    > 删除del_timer O(1)
    > 执行tick()    O(1)



最小堆是一种完全二叉树, 可以用数组存储元素
>* 节省空间(避免了指针)
>* 可以直接访问,插入和删除更为容易
>* 但是需要一块连续的空间
>* 初始化两种方法:
    >>* 1. 初始化一个空堆,将数组的每个元素插入堆中,(效率偏低)
    >>* 2. 只对数组第(N-1)/2 ~ 0个元素进行下虑操作,即可保证该数组构造一个最小堆
            >>>* 对于N个元素的完全二茶树, 具有(N-1)/2个非叶子节点, 这些叶子节点正是该完全二叉树地0~(N-1)/2个节点
            >>>* 只要确保这些非叶子节点具有堆序性质,整个树就会具有堆序性质

*/

#ifndef MIN_HEAP
#define MIN_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>

using std::exception;

const int BUFFER_SIZE = 64;

class heap_timer; //前向声明

//用户数据
struct client_data
{
    sockaddr_in address; //客户端socket地址
    int sockfd;          //socket文件描述符
    heap_timer *timer;   //最小堆定时器
    char buf[BUFFER_SIZE];
};

//定时器类
class heap_timer
{
public:
    time_t expire;                  //定时器生效的绝对时间
    void (*cb_func)(client_data *); //回调函数
    client_data *user_data;         //客户数据

public:
    heap_timer(int delay)
    {
        expire = time(NULL) + delay;
    }
};

//时间堆类
class time_heap
{
private:
    heap_timer **array; //最小堆定时器数组
    int capacity;       //堆数组的容量
    int cur_size;       //堆数组当前的包含的元素个数

public:
    //构造函数一:指定堆数组容量
    time_heap(int cap) : capacity(cap), cur_size(0)
    {
        array = new heap_timer *[capacity];
        if (!array)
        {
            throw std::exception();
        }
        for (int i = 0; i < capacity; ++i)
        {
            array[i] = nullptr;
        }
    }

    //构造函数二:有已有定时器初始化堆
    time_heap(heap_timer **init_array, int size, int capacity) : cur_size(size), capacity(capacity)
    {
        if (capacity < size)
        {
            printf("capacity < size\n");
            std::exception();
        }
        array = new heap_timer *[capacity];
        if (!array)
        {
            printf("new array failed\n");
            std::exception();
        }

        for (int i = 0; i < capacity; ++i)
        {
            array[i] = nullptr;
        }

        if (size != 0)
        {
            //初始化堆数组
            for (int i = 0; i < size; ++i)
            {
                array[i] = init_array[i];
            }
            //对数组中(cur_size-1)/2 ~ 0个元素执行下虑操作,确保有序 ,
            for (int i = (cur_size) / 2; i >= 0; --i)
            {
                percolate_down(i); //下虑操作: 将孔穴与其最小的子节点交换
            }
        }
    }

    ~time_heap()
    {
        for (int i = 0; i < cur_size; ++i)
        {
            delete array[i];
            array[i] = nullptr;
        }
        delete[] array;
    }

public:
    //添加目标定时器timer  O(lg[n])
    void add_timer(heap_timer *timer) //上虑操作:将空穴与其父节点交换
    {
        if (!timer)
        {
            return;
        }
        if (cur_size >= capacity)
        {
            resize();
        }

        int hole = cur_size++;          //新插入一个元素,cur_size + 1
        int parent = 0;                 //hole是新建空穴的位置
        for (; hole > 0; hole = parent) //对从空穴到跟节点路径上所有节点执行上虑操作
        {
            parent = (hole - 1) / 2; //父节点的下标
            if (array[parent]->expire <= timer->expire)
            {
                break;
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }

    //删除目标定时器 O(1)
    void del_timer(heap_timer *timer)
    {
        if (!timer)
        {
            return;
        }

        /*只将目标定时器的回调函数置空[延迟销毁] 
            > 节省真正删除该定时器的开销, 因为采用的是数组存储,连续空间,删除时涉及到位置移动,会有一定的开销(自己的理解)
            > 但是这样容易导致数据膨胀,尤其是定时器不断增加而不删除的话
                > 解决? 是否可以开一个线程定期删除呢??(因为这样置空的话是在数组中间有nullptr值,如下,很容易和最后一个元素之后的nullptr区分)
                    >* 21, 24, nullptr, 65, 26, nullptr, nullptr, 
        */
        timer->cb_func = nullptr;
    }

    //获取堆顶的定时器
    heap_timer *top() const
    {
        if (empty())
        {
            return nullptr;
        }
        return array[0];
    }

    //删除堆顶元素
    void pop_timer()
    {
        if (empty())
        {
            return;
        }
        if (array[0])
        {
            delete array[0];              //删除堆顶
            array[0] = array[--cur_size]; //将原来堆顶元素替换为堆数组中最后一个元素,cur_size--;
            percolate_down(0);            //下虑操作
        }
    }

    //判空
    bool empty() const
    {
        return cur_size == 0;
    }

    //滴答函数 执行O(1)
    void tick() 
    {
        heap_timer *temp = array[0]; //堆顶元素(数组第一个元素)是expire时间最小的元素
        time_t cur = time(NULL);
        while (!empty()) //循环处理堆中到期的定时器
        {
            if (!temp)
            {
                break;
            }
            if (temp->expire > cur) //没有到期,退出循环
            {
                break;
            }

            if (array[0]->cb_func) //执行定期任务 执行O(1)
            {
                array[0]->cb_func(array[0]->user_data);
            }
            pop_timer(); //删除堆顶元素同时生成新的堆顶定时器
            temp = array[0];
        }
    }

private:
    //下虑操作: 将hole与其最小的子节点交换,确保数组中以第hole个节点为根的子树拥有最小堆性质
    void percolate_down(int hole)
    {
        heap_timer *temp = array[hole];
        int child = 0;
        for (; ((hole * 2 + 1) <= cur_size - 1); hole = child)
        {
            child = 2 * hole + 1;
            if (child < (cur_size - 1) && (array[child + 1]->expire < array[child]->expire))
            {
                ++child;
            }
            if (array[child]->expire < temp->expire)
            {
                array[hole] = array[child];
            }
            else
            {
                break;
            }
        }
        array[hole] = temp;
    }

    //2倍扩容
    void resize() throw()
    {

        heap_timer **temp = new heap_timer *[2 * capacity];
        for (int i = 0; i < 2 * capacity; ++i)
        {
            temp[i] = nullptr;
        }
        if (!temp)
        {
            throw ::exception();
        }
        capacity = 2 * capacity;
        for (int i = 0; i < cur_size; ++i)
        {
            temp[i] = array[i];
        }
        delete[] array;
        array = temp;
    }
};

#endif
