/*
 * @Author: hancheng 
 * @data: 2020-07-12 15:46:09 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-12 22:52:41
 */

/*
定时器之时间轮:
槽间隔(slot interval) 插入槽(time slot) N个槽
定时器ti插入的位置 ts = (cs + (ti/si)) % N 
时间轮有点哈希表的意思[将定时器散列到不同的槽对应的链表上] : 每个槽对应一个链表,一个新来的定时器,先计算槽的位置,然后将其对应槽的链表中
si越小,定时器精度越高, 一般可以好几个轮,类似水表
本时间论只有一个时轮子

>  复杂度:
    > 添加add_timer O(1)
    > 删除del_timer O(1)
    > 执行tick()    O(n)[最坏]
        > 实际执行一个定时器效率比O(n)好得多,因为时间轮所有定时器散列到不同的链表中去
        > 时间轮的槽越多,等价于散列表入口越多,从而每条链表上的时间定时器数量越少 --> 有点类似散列表的链地址法
        >  使用多个轮子来实现时间轮时,执行时间(tick())会大大减少,可能接近O(1)
*/

#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <time.h>
#include <stdio.h>
#include <netinet/in.h>

const int BUF_SIZE = 64; //循环数组大小

class tw_timer;

//用户数据结构
struct client_data
{
    sockaddr_in address; //客户端socket地址
    int sockfd;          //socket文件描述符
    tw_timer *timer;     //定时器
};

//定时器类
class tw_timer
{
public:
    int rotation;                   //记录定时器转动多少圈后生效
    int time_slot;                  //记录定时器在时间轮哪个槽上
    void (*cb_func)(client_data *); //定时器回调函数
    client_data *user_data;         //客户数据
    tw_timer *next;                 //指向下一个定时器
    tw_timer *prev;                 //指向上一个定时器

public:
    tw_timer(int rot, int ts) : next(nullptr), prev(nullptr), rotation(rot), time_slot(ts) {}
};

//时间轮：利用在同一个槽链表上的定时器相差 时间轮的 整数倍的特性
class time_wheel
{
private:
    static const int N = 60; //时间轮上槽的槽数
    static const int SI = 1; //槽间间隔 1s,每秒钟转动一次
    tw_timer *slots[N];      //时间轮的槽,每个槽对于一个链表(无序)
    int cur_slot;            //时间轮上当前槽位置

public:
    time_wheel() : cur_slot(0)
    {
        for (int i = 0; i < N; ++i)
        {
            slots[i] = nullptr; // 初始化每个槽的头节点
        }
    }

    ~time_wheel()
    {
        for (int i = 0; i < N; ++i) //遍历每个槽,销毁其中定时器
        {
            tw_timer *temp = slots[i];
            while (temp)
            {
                slots[i] = temp->next;
                delete temp;
                temp = slots[i];
            }
        }
    }

    //根据指定的timeout创建一个定时器,并插入对应的槽中,以123.6s为例
    //插入 O(1)
    tw_timer *add_timer(int timeout)
    {
        if (timeout < 0)
        {
            return nullptr;
        }
        int ticks = 0; //间隔的倍数
        if (timeout < SI)
        {
            ticks = 1; //向上取整
        }
        else
        {
            ticks = timeout / SI; //向下取整  ticks = 123
        }
        int rotation = ticks / N;              //定时器在多少圈后生效 rotation = 2;
        int ts = (cur_slot + (ticks % N)) % N; //定时器插入的位置 ticks % N = 3;

        tw_timer *timer = new tw_timer(rotation, ts); //创建定时器,在时间轮的第ts个槽中,在时间论转动rogation圈后生效

        if (!slots[ts]) //第slots[ts]个槽为空.作为头节点插入
        {
            // printf("add timer, rotation:%d, ts:%d, cur_slot:%d\n", rotation, ts, cur_slot);
            slots[ts] = timer;
        }
        else //头插法
        {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }

        return timer;
    }

    //删除定时器  O(1)
    void del_timer(tw_timer *timer)
    {
        if (!timer)
        {
            return;
        }

        int ts = timer->time_slot; //目标定时器在时间轮的位置ts(在第ts个链表上)
        if (timer == slots[ts])    //若是链表头
        {
            slots[ts] = slots[ts]->next;
            if (slots[ts])
            {
                slots[ts]->prev = nullptr;
            }
            delete timer;
        }
        else
        {
            timer->prev->next = timer->next;
            if (timer->next)
            {
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    //滴答函数: SI时间后,调用该函数,时间论轮向前滚动一个槽的间隔  执行 O(n)有一个遍历过程
    //执行过程如下：查看当前槽上的定时器轮数是否为 0,不是，则轮数-1.执行下一个
    //执行O(n)
    void tick()
    {
        tw_timer *temp = slots[cur_slot];
        // printf("currrent slot is %d\n", cur_slot);
        while (temp)
        {
            // printf("tick the timer onece\n");
            if (temp->rotation > 0) //若定时器的rotation > 0 ,则在这一轮中不起作用.
            {
                temp->rotation--;
                temp = temp->next;
            }
            else //否则,该定时器已经到期,需要执行定时任务,然后删除该定时器.
            {
                temp->cb_func(temp->user_data);
                if (temp == slots[cur_slot])
                {
                    // printf("delete header in cur_slot:%d\n", cur_slot);
                    slots[cur_slot] = temp->next;
                    delete temp;
                    if (slots[cur_slot])
                    {
                        slots[cur_slot]->prev = nullptr;
                    }
                    temp = slots[cur_slot];
                }
                else
                {
                    temp->prev->next = temp->next;
                    if (temp->next)
                    {
                        temp->next->prev = temp->prev;
                    }
                    tw_timer *temp2 = temp->next;
                    delete temp;
                    temp = temp2;
                }
            }
        }
        cur_slot = (cur_slot + 1) % N; //更新时间论的当前槽,以表示时间轮的转动
    }
};

#endif
