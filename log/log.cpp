/*
 * @Author: hancheng 
 * @Date: 2020-07-11 15:52:23 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-13 13:52:19
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>

#include "log.h"

using namespace std;

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{
    if (nullptr != m_fp)
    {
        float(m_fp);
        m_fp = 0;
    }
}

//获取实例
Log *Log::get_instance()
{
    // 局部静态变量的方式实现单例模式, 线程安全
    //      -> 原因:C++11标准规定:当一个线程正在初始化一个变量时,其他线程必须得等到该初始化完成后才能访问
    static Log instance;
    return &instance;
}

//写入日志的函数(回调函数)
void *Log::flush_log_thread(void *args)
{
    Log::get_instance()->async_write_log();
}

//异步写日志
void *Log::async_write_log()
{
    string single_log;
    //从阻塞队列中取出一个日志string，写入文件
    while (m_log_queue->pop(single_log))
    {
        m_mutex.lock();
        fputs(single_log.c_str(), m_fp);
        m_mutex.unlock();
    }
}

//初始化, 日志文件名前缀是时间 后缀是自定义的log文件名
bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size)
{
    //判断日志写入方式:异步(max_queue_size > 0 ) 或 同步(max_queue_size = 0)
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;

        //创建线程,异步写入日志文件,flush_log_thread为回调函数
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    //日志最大行数
    m_split_lines = split_lines;

    //获取log文件名
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    //strrchr(const char* str, int ch) 从后往前找第一个 '/' 出现的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0}; //日志文件名

    //自定义文件名, 若p为NULL表示没有文件名,则直接将 时间 + 文件名 作为日志文件名
    if (nullptr == p)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        //给 日志文件名 添加后缀
        strcpy(log_name, p + 1);
        //p - filename + 1 是文件所在路径文件夹的长度
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a"); //追加写入
    if (nullptr == m_fp)
    {
        return false;
    }

    return true;
}

//写入日志(默认同步)
void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    ++m_count;

    //每日日志判断或日志超过指定的最大行数时(5000000行),则新建日志文件
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {

        char new_log[256] = {0};
        fflush(m_fp); //更新缓冲区,强制将缓冲区内容写入m_fp中
        fclose(m_fp);

        char tail[16] = {0};
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday) //若时间不是今天,则创建今天的日志且同时更新m_today和m_count
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else //超过了最大行数
        {
            //新命名日志文件名,在之前的文件名后加上后缀 m_count / m_split_lines
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }

        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();

    va_list valst;           //类型，存储可变参数的信息
    // 将传入的format参数赋值给valst,便于格式化输出
    va_start(valst, format); //宏定义，开始使用可变参数列表

    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式: 时间 + 内容
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    //int vsnprintf( char* buffer, std::size_t buf_size, const char* format, va_list vlist ) (C++11 起)
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    
    //根据写入日志方式,进行处理
    if (m_is_async && !m_log_queue->full())//异步日志:放入队列,然异步日志线程将log_str写入日志文件
    {
        m_log_queue->push(log_str);
    }
#ifdef DEBUG
    else if(!m_is_async) //这里才是真正的同写入呀
    {
          m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
#endif // DEBUG
    //同步日志,立即写入日志文件   
    else /* 这里代码导致[ 若是异步但是阻塞队列满了,还是会采用同步方式写入呀]?? */
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
   
    va_end(valst); // 宏定义，结束使用可变参数列表
}

//强制将缓冲区内容写m_fp
void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
