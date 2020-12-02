#ifndef LOG_H_
#define LOG_H_

#include <mutex>
#include <thread>
#include <string>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <sys/time.h>
#include <sys/stat.h>

#include "block_queue.h"
#include "buffer/buffer.h"




/**
 * 涉及知识点：
 *    deque
 *    锁：unique_lock、 lock_guard
 *    智能指针 ： unique_ptr
 *    单例模式： 饱汉模式与饿汉模式，C++11新的初始化方法
 *    时间 ： timer
 *    可变参数
 *    C++11 的线程类 thread 
 *    虚函数 ：虚析构函数
 *    库函数：fputs()、fflush()、snprintf()
*/

// 日志 Log 类
class Log
{
    public:
        void init(int level, const char *path = "./log",
                  const char *suffix = ".log",
                  int queueMaxCapacity = 1024);

        static Log *instance();
        static void flushLogThread();

        void write(int level, const char *format, ...);
        void flush();

        int getLevel();
        void setLevel(int level);
        bool isOpen();

    private:
        Log();
        virtual ~Log();
        void appendLogLevelTitle(int level);
        void asyncWrite();

    private:
        static const int LOG_PATH_LEN = 256;
        static const int LOG_NAME_LEN = 256;
        static const int LOG_MAX_LINE = 50000;

        const char *log_path;
        const char *log_suffix;

        int log_max_line;
        int lineCount;
        int today;

        bool log_isOpen;
        Buffer buff;
        int log_level;
        bool isAsync; // 是否异步

        FILE *fp;

        std::mutex mtx;
        std::unique_ptr<BlockQueue<std::string>> logDeque_ptr;
        std::unique_ptr<std::thread> writeThread_ptr;

};

#define LOG_BASE(level, format, ...)\
    do{\
        Log *log = Log::instance();\
        if (log->isOpen() && log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__);\
            log->flush();\
            }\
     }while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);

#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);

#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);

#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif