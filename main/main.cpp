/*
 * @Author: hancheng 
 * @Date: 2020-07-12 14:13:23 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-25 21:09:43
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>


#include "../lock/locker.h"
#include "../threadpool/threadpool.h"

// #include "./timer/lst_timer.h"
#include "../timer/timewheel.h"

#include "../http/http_conn.h"
#include "../log/log.h"
#include "../CGImysql/sql_connection_pool.h"

#include "../utils/createTable.h"

const int  MAX_FD = 65536  ; //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;//最小超时单位

#define SYNLOG  //同步写日志
//#define ASYNLOG //异步写日志

//#define listenfdET //边缘触发非阻塞
#define listenfdLT //水平触发阻塞

//这三个函数在http_conn.cpp中定义，改变链接属性
extern int addfd(int epollfd, int fd, bool one_shot);
extern int remove(int epollfd, int fd);
extern int setnonblocking(int fd);

/*信号处理函数利用管道通知主循环执行定时器链表上的定时任务*/
//设置定时器相关参数
static int pipefd[2]; 

//创建定时器
// static sort_timer_lst timer_lst;
static time_wheel tw;

static int epollfd = 0;

/**
 * 信号处理函数:这里采用统一事件源,信号处理函数只起到发信号的作用
 * 这里的信号处理函数只向管道写入数据, 信号处理逻辑在主循环实现
 * 主循环通过epoll监听,管道是否可读,
 */
void sig_handler(int sig)
{
    int save_errno = errno;  //保留原来的errno,函数最后恢复,以保证函数的可重入性
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0); //将信号写入管道,以通知主循环
    errno = save_errno;
}

//设置信号的处理函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART; //SA_RESTART 重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask); //在信号集中设置所以信号
    assert(sigaction(sig, &sa, NULL) != -1);
}


//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
    // timer_lst.tick(); //定时处理任务,实际上就是调用tick函数
    tw.tick();
    alarm(TIMESLOT); //因为一次alarm调用只会引起一次SIGALRM信号,所以我们需要重新定时,以不断触发SIGALRM信号
}

//定时器回调函数，删除非活动连接在socket上的注册事件，并关闭
//从内核事件表删除事件并关闭文件描述符,释放占用的连接资源
void cb_func(client_data *user_data)
{
    //从内核事件表删除事件
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd); //关闭文件描述符,释放占用的连接资源
    //连接数-1
    http_conn::m_user_count--;
    LOG_INFO("close fd %d", user_data->sockfd);
    Log::get_instance()->flush();
}

void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char *argv[])
{
#ifdef ASYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 8); //异步日志模型
#endif

#ifdef SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0); //同步日志模型
#endif

    if (argc <= 1)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    int port = atoi(argv[1]);

    addsig(SIGPIPE, SIG_IGN);
    
    // //创建数据库连接池
    // connection_pool *connPool = connection_pool::GetInstance();
    // connPool->init("localhost", "root", "root", "njpdb", 3306, 8);
//
//创建数据库连接池
    string url = "localhost";
    string nameuser = "root";
    string passwd = "root";
    int dbport = 3306;
    unsigned int maxConn = 8;
    //提前安装mysql
    string dbname =  "tinywebdb";//"tinywebdb";
    string tbname = "userinfo";
    create(url, nameuser, passwd,dbport, dbname, tbname); //建立数据库和表

    //连接池
    connection_pool *connPool = connection_pool::GetInstance();
     connPool->init(url,  nameuser, passwd, dbname, 3306, 8);
    // connPool->init("localhost", "root", "root", "njpdb", 3306, 8);

//  
    //创建线程池
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>(connPool);
    }
    catch (...)
    {
        return 1;
    }

    //创建MAX_FD个http类对象,预先为每个可能的客户连接分配一个http_conn对象
    http_conn *users = new http_conn[MAX_FD]; //创建连接资源数组
    assert(users);
    /*
    //初始化数据库读取表
    users->initmysql_result(connPool);
    */

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    //struct linger tmp={1,0};
    //SO_LINGER若有数据待发送，延迟关闭
    //setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    //创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    //将listenfd添加到epollfd上
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    //创建一对全双工套接字
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    /*设置管道写端为非阻塞, 为什么写端非阻塞(??因为send将信息发送给套接字缓冲区,
    如果缓冲区满了则会阻塞,这是会进一步增加信号处理的执行时间,因此设置为非阻塞的.)*/
    setnonblocking(pipefd[1]);

    //设置管道读端为ET非阻塞
    addfd(epollfd, pipefd[0], false);

    //传递给主循环的信号值 只关注[SIGALRM 和 SIGTERM]
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);

    //循环标识
    bool stop_server = false;

    client_data *users_timer = new client_data[MAX_FD];

    //超时标识
    bool timeout = false; //timeout用来表示是否有定时任务需要处理
    
    //每隔TIMESLOT时间触发SIGALRM信号
    alarm(TIMESLOT);//alarm定时触发SIGALRM信号

    while (!stop_server)
    {
        //调用epoll_wait监听事件,监听所有文件描述符上事件的产生
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        
        //errno == EINTR表示被中断
        if (number < 0 && errno != EINTR) 
        {
            LOG_ERROR("%s", "epoll failure, errno:%d", errno);
            break;
        }

        //轮询所有已就绪的文件描述符
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            // int whatopt = events[i].events;

            //事件一:处理新到的客户连接请求
            if (sockfd == listenfd)
            {
                //保存用户数据 socket地址和对应的connnfd连接
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);

//level trigged 水平触发
#ifdef listenfdLT
                // 该连接分配的文件描述符
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    LOG_ERROR("accept error, errno is:%d", errno);
                    continue;
                }
                //用户数量超过最大描述符,直接拒绝
                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                //初始化客户数据,然后添加到任务队列中
                users[connfd].init(connfd, client_address);

                //初始化client_data数据
                users_timer[connfd].address = client_address;
                users_timer[connfd].sockfd = connfd;

                
                /*  基于升序链表的定时器
                 创建定时器,设置其回调函数和超时时间,然后帮顶定时器与用户数据,最后将定时器添加到链表timer_lst中
                util_timer *timer = new util_timer; //创建定时器临时变量
                timer->user_data = &users_timer[connfd]; //设置定时器对应的连接资源
                timer->cb_func = cb_func; //设置回调函数
                time_t cur = time(NULL); //获取当前时间(绝对时间)
                timer->expire = cur + 3 * TIMESLOT; //设置超时时间(绝对时间)
                users_timer[connfd].timer = timer;为新建立的连接设置定时器
                timer_lst.add_timer(timer);//将定时器添加到链表中
                */


                /*基于时间论的定时器*/
                time_t cur = time(NULL);
                tw_timer* timer = tw.add_timer( cur + 3 *TIMESLOT);
                timer->user_data = &users_timer[connfd];
                timer->cb_func = cb_func;

                users_timer[connfd].timer = timer; //添加到定时器结构中

#endif

// edge triger边缘触发
#ifdef listenfdET //非阻塞边缘触发
                while (1)
                {
                    int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                    if (connfd < 0)
                    {
                        LOG_ERROR("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (http_conn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        LOG_ERROR("%s", "Internal server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_address);

                    //初始化client_data数据
                    users_timer[connfd].address = client_address;
                    users_timer[connfd].sockfd = connfd;
                    /*
                    创建定时器,设置其回调函数和超时时间,然后帮顶定时器与用户数据,最后将定时器添加到链表timer_lst中
                    util_timer *timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    time_t cur = time(NULL);
                    timer->expire = cur + 3 * TIMESLOT;
                    users_timer[connfd].timer = timer;
                    timer_lst.add_timer(timer);
                    */

                    /*基于时间论的定时器*/
                    time_t cur = time(NULL);
                    tw_timer* timer = tw.add_timer( cur + 3 *TIMESLOT);
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;

                    users_timer[connfd].timer = timer;
                }
                continue;
#endif
            }

            //事件二:对端关闭连接事件(异常事件)
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                // util_timer *timer = users_timer[sockfd].timer;
                tw_timer* timer = users_timer[sockfd].timer;
                timer->cb_func(&users_timer[sockfd]);

                if (timer)
                {
                    tw.del_timer(timer);
                    // timer_lst.del_timer(timer);
                }
            }

            //事件三: 读事件且是信号, 这里采用统一事件源,把信号处理当成普通I/O事件处理, 主循环和工作线程通过管道传递信号值
            //处理信号 如果就绪的文件描述符是pipefd[0]则处理信号  <--- 统一信号源: 把信号事件和其他I/O事件一样处理
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                //从管道读端读出信号值,成功返回字节数,失败返回-1
                //正常情况下,ret返回值总是1,只有14和15的ACSII码对应的字符
                ret = recv(pipefd[0], signals, sizeof(signals), 0); //主循环从管道中接收信号值
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else //信号处理逻辑在这里实现
                {
                    //因为每个信号值占1个字节,所以按字节逐个接受信号
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        { 
                        case SIGALRM: //由alarm或setitimer设置的实时闹钟信号超时引起
                        {
                            //timeout表示是否有定时任务需要处理,但并不是立即处理,因为定时任务优先级并不是很高,我们优先处理优先级别高的任务
                            timeout = true;  //接收到SIGALRM信号，timeout设置为True
                            break;
                        }
                        case SIGTERM: //终止进程(kill)
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }
            
            //事件四: 读事件且是客户端发送的请求 : 处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                // util_timer *timer = users_timer[sockfd].timer;
                tw_timer* timer = users_timer[sockfd].timer;
                if (users[sockfd].read_once()) 
                {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();
                    //若监测到读事件，将该事件放入请求队列
                    pool->append(users + sockfd);

                    //若该连接有数据传输, 则将该连接的定时器往后延迟 3 个 单位,并调整定时器的位置
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        // timer->expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        // timer_lst.adjust_timer(timer);
                        tw.add_timer(cur + 3 * TIMESLOT);
                    }
                }
                else//对端关闭连接
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        // timer_lst.del_timer(timer);
                        tw.del_timer(timer);
                    }
                }
            }

            //事件五:写事件 
            else if (events[i].events & EPOLLOUT)
            {
                // util_timer *timer = users_timer[sockfd].timer;
                tw_timer* timer = users_timer[sockfd].timer;
                if (users[sockfd].write()) //write把缓冲区响应报文发给客户端
                {
                    LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
                    Log::get_instance()->flush();

                    //若有数据传输，则将定时器往后延迟3个单位
                    //并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        // timer->expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::get_instance()->flush();
                        // timer_lst.adjust_timer(timer);
                        tw.add_timer(cur + 3 * TIMESLOT);
                    }
                }
                else //write发送给浏览器失败,则关闭连接
                {
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        // timer_lst.del_timer(timer);
                        tw.del_timer(timer);
                    }
                }
            }
        }
        //处理定时器为非必须事件,收到信号后并不立即处理, 读写完成后再进行处理
        //先处理优先级高的任务,因为处理定时事件需要耗时
        if (timeout)//最后处理定时任务
        {
            timer_handler();
            timeout = false;
        }
    }
    
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete pool;
    return 0;
}