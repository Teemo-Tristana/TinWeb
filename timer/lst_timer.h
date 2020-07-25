/*
 * @Author: hancheng 
 * @Date: 2020-07-11 22:15:45 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 11:09:47
 */

//这里用的是基于升序链表的定时器,可以尝试基于时间轮/堆的定时器

/*
定时器模块:
> 定时: 在一段时间之后触发某段代码的机制
        非活动连接占用资源,严重影响服务器性能, 通过实现一个服务器定时器,处理这种非活动连接,释放资源.
        利用alarm函周期触发SIGALRM信号,该信号通过管道同志主循环指定定时器链表的定时任务.

> 定时器必有成员:
     > 超时时间
     > 回调函数

> 每间隔一段时间就执行一次,一检测并处理到期的任务
    > 到期的依据: 定时器的expire值小于 当前的系统时间(绝对时间和相对时间均可,我们这里采用绝对时间)

> 不足:
    添加定时器的效率较低, 随着定时器数目增加而降低 O(n)

>  复杂度:
    > 添加add_timer O(n) 每次添加需要遍历找到合适的位置 ,随着定时器的增加性能会下降
    > 删除del_timer O(1)
    > 执行tick()    O(1)
*/

#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include "../log/log.h"

class util_timer; //前向声明

//用户数据结构
struct client_data
{
    sockaddr_in address; //客户端socket地址
    int sockfd;          //socket文件描述符
    util_timer *timer;   //定时器
};

//定时器类
//将定时事件作为回调函数,封装起来由用户定义,[这里只用来删除非活动连socket上的注册事件并关闭]
class util_timer
{
public:
    time_t expire;                  //超时时间 [绝对时间]
    void (*cb_func)(client_data *); //回调函数()
    client_data *user_data;         //用户数据
    util_timer *prev;               //指向前一个定时器
    util_timer *next;               //指向后一个定时器

public:
    util_timer() : prev(nullptr), next(nullptr) {}
};

//定时器容器类: 这里是定时器容器是链表, 基于升序的带有头尾节点的双向链表
class sort_timer_lst
{
private:
    util_timer *head;
    util_timer *tail;

public:
    sort_timer_lst() : head(nullptr), tail(nullptr) {}

    //定时器被销毁时,删除其中所有定时器
    ~sort_timer_lst()
    {
        util_timer *temp = head;
        while (temp)
        {
            head = temp->next;
            delete temp;
            temp = head;
        }
    }

    //添加节点(定时器)到链表 O(n)
    void add_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if (!head)
        {
            head = tail = timer;
            return;
        }
        /*如果新的定时器超时时间小于当前头部结点,直接将当前定时器结点作为头部结点*/
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        //调用add_timer(),在表头以外的找到合适的位置插入
        add_timer(timer, head);
    }

    /*当某个定时任务发生变化后,调整对应的定时器在链表中的位置, 
    本函数之考虑被调整的定时器的超时时间延长的情况[将该定时器往链表尾部移动]*/
    //调整定时器，任务发生变化时，调整定时器在链表中的位置
    void adjust_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        util_timer *temp = timer->next;
        //被调整的目标定时器在链表尾或该定时器新的定时时间仍然小于其下一个定时器的超时时间,则不用调整
        if (!temp || (timer->expire < temp->expire))
        {
            return;
        }
        //如果定目标定时器是表头,则将该定时器从链表中取下,并重新插入链表
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head); //取下timer,往后移动
        }
        //如果定目标定时器不是表头,则将该定时器从链表相应位置取下,并重新插入其之后的链表位置中.
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next); //取下timer,往后移动
        }
    }

    //将目标定时器timer从链表中删除
    void del_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if ((timer == head) && (timer == tail)) //即整个链表就剩下一个结点，直接删除
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }

        if (timer == head) //被删除的定时器为头结点
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail) //被删除的是尾结点
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        //不是头尾，普通删除
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
        return;
    }

    /*定时任务处理函数: SIGALRM信号每次被触发时,就在其信号处理函数[若是使用统一事件源,
      则是主函数]中执行一次tick(), 以处理链表中到期的任务
      
      思路: 
        遍历定时器升序链表,从头节点开始依次处理每个定时器,直到遇到尚未到期的定时器
        若当前时间小于超时时间,跳出循环,即未找到到期的定时器
        若当前时间大于定时器的超时时间[绝对时间],即找到到期的定时器,则执行回调函数,然后将其从链表中删除,然后继续遍历
    */
    
    void tick() //滴答函数(核心函数,定时器到期后的处理是由tick()函数调用执行的)
    {
        if (!head)
        {
            return;
        }

        // printf("timer tick\n");
        LOG_INFO("%s", "timer tick");
        Log::get_instance()->flush();
        time_t cur = time(NULL);
        util_timer *temp = head;
        //从头节点开始往后依次处理每个定时器, 知道遇到一个尚未到期的定时器
        while (temp)
        {
            //定时器为到期
            if (cur < temp->expire) /*定时器使用的是绝对时间,因此可以把定时器的超时时间和系统当前时间进行比较以判断定时器是否到期*/
            {
                break;
            }

            //定时器已到期,则进行处理

            //调用定时器的回调函数,以执行定时任务
            temp->cb_func(temp->user_data);
            //执行完定时任务之后,就将定时器从链表中删除,并重置链表表头[ps:本定时器是一个基于链表的升序定时器]
            head = temp->next;
            if (head)
            {
                head->prev = NULL;
            }

            delete temp;
            temp = head;
        }
    }

private:
    //将目标定时器timer 添加到 lst_head之后的位置
    void add_timer(util_timer *timer, util_timer *lst_head) //重载的add_timer()函数
    {
        util_timer *prev = lst_head;
        util_timer *temp = prev->next;
        //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
        while (temp)
        {
            if (timer->expire < temp->expire) //插入到prev后，tmp之前
            {
                prev->next = timer;
                timer->next = temp;
                temp->prev = timer;
                timer->prev = prev;
                break;
            }

            prev = temp;
            temp = prev->next;
        }

        if (!temp) //插入链表尾部
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }
};

#endif
