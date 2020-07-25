/*
 * @Author: hancheng 
 * @Date: 2020-07-11 14:17:02 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 11:34:15
 */
/*
日志模块:
功能: 在服务器运行期间可以打印日志到日志文件
      是多线程安全的单例模式, 支持按天分文件,按日志行数自动分文件
      安全级别高时采用同步日志方式[不能丢失日志]
      安全级别不高时采用异步日志模式(异步模式在程序异常崩溃或重启服务器时可能出现日志丢失)
      异步模式采用阻塞队列实现

      
>* 同步日志
>* 异步日志: 能够顺利写日志同时不占用主线程时间(日志书写和主线程执行分开)
    >* 文件I/O速度较慢,因此我们采用异步I/O先写入内存(我们这里用自建数组实现的队列来存储),然后日志线程自己读取该列表然后写入文件
>关键问题:
    > 日志队列:  主线程往队列中写(非满的情况),日志线程从队列中取(非空情况)  <----> 生产者与消费者模型
        >> 需要一个互斥锁和信号量,操作前加锁,完毕后解锁
    > 写日志的线程:
        >> 新建一个线程(消费者),不断while,当日志队列非空时,就从中取出写入文件(操作队列需要加锁)


知识点: 线程, 线程锁, 条件变量, STL的queue
思路: 提供一个接口, 可以直接调用并打印日志到日志文件,但是不能影响服务器正常运行[不能进行大量的文件操作]
流程: 
    对象初始化时创建一个线程在后台读取队列
    将队列数据写入日志系统并在队列删除该数据
    接口处:每次写入日志时,其实是将日志存到队列中,交给后台线程写入日志文件中

难点: 
    1.临界区的控制(线程锁,条件变量的使用)
    2.对deque的理解和使用
    3.C++在类内部实现实例化

两大部分:
    >* 单例模式 + 阻塞队列
    >* 日志类的定义与使用


*/

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>

#include "block_queue.h" //阻塞队列

using namespace std;

//Log使用单例模式的饱汉模式
class Log
{
private:
    char dir_name[128];               //路径名
    char log_name[128];               //log文件名
    int m_split_lines;                //日志文件最大行数
    int m_log_buf_size;               //日志缓冲区大小
    int m_today;                      //因为按天分类,记录是那一天
    long long m_count;                //记录已有日志行数
    bool m_is_async;                  //是否异步标志位
    block_queue<string> *m_log_queue; //阻塞队列
    locker m_mutex;                   //互斥锁
    FILE *m_fp;                       //打开log的文件指针
    char *m_buf;                      //输出内容缓冲区

public:
    //在C++11内部, 使用局部变量懒的懒汉模式是线程安全的,因此只被初始化一次
    static Log *get_instance();

    //异步写入日志[调用私有方法async_write_log]
    static void *flush_log_thread(void *args);

    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    //将输出内容按照标准格式整理
    void write_log(int level, const char *format, ...);

    //强制刷新缓冲区
    void flush(void);

private:
    Log();

    virtual ~Log();

    //异步写日志
    void *async_write_log();
};


/*********************** 宏定义************************/
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

#endif
