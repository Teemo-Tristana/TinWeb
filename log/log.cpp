#include "log.h"

using namespace std;

// 
Log::Log() : lineCount(0),today(0), isAsync(false),fp(nullptr),
             logDeque_ptr(nullptr), writeThread_ptr(nullptr){}


//析构函数
Log::~Log(){
    // 是否异步(异步才开启额外线程进行写入)
    if(writeThread_ptr && writeThread_ptr->joinable())
    {
        while (!logDeque_ptr->empty())
        {
            logDeque_ptr->flush();
        }

        logDeque_ptr->close();
        writeThread_ptr->join();
    }

    if (fp != nullptr)
    {
        std::lock_guard<std::mutex> locker(mtx);
        flush();
        fclose(fp);
    }
}

// 添加日志级别名称
void Log::appendLogLevelTitle(int level)
{
    switch(level)
    {
        case 0 : 
            buff.append("[debug]: ", 9);
            break;

        case 1 : 
            buff.append("[info]: ", 8);
            break;

        case 2 : 
            buff.append("[warn]: ", 8);
            break;

        case 3 : 
            buff.append("[error]: ", 9);
            break;
        
        default : 
            buff.append("[unkonw]: ",10);
            break;
    }
}

// 异步写入
void Log::asyncWrite()
{
    std::string str = "";
    while (logDeque_ptr->pop(str))
        {
            std::lock_guard<std::mutex> locker(mtx);
            fputs(str.c_str(), fp);
        }
}

// 日志类初始化
void Log::init(int level, const char *path, const char *suffix,int queueMaxCapacity)
{
    log_isOpen = true;
    log_level = level;
    // 异步
    if (queueMaxCapacity > 0)
    {
        isAsync = true;
        if (!logDeque_ptr)
        {
            std::unique_ptr<BlockQueue<std::string>> tempQueue(new BlockQueue<std::string>);
            logDeque_ptr = move(tempQueue);

            std::unique_ptr<std::thread> tempThread(new thread(flushLogThread));
            writeThread_ptr = move(tempThread);
        }
    }
    // 同步
    else
        isAsync = false;

    lineCount = 0;
    time_t timer = time(nullptr);
    struct tm* sysTime = localtime(&timer);
    struct tm t = *sysTime;

    log_path = path;
    log_suffix = suffix;

    char filename[LOG_NAME_LEN] = {0};
    snprintf(filename, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             log_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, log_suffix);

    today = t.tm_mday;

    {
        std::lock_guard<std::mutex> locker(mtx);
        buff.retrieveAll();
        if (fp != nullptr)
        {
            mkdir(log_path, 0777);
            fp = fopen(filename, "a");
        }

        assert(fp != nullptr);
    }
}

// 实例化日志
Log* Log::instance()
{
    static Log inst;
    return &inst;
}

// 写入日志的线程的函数
void Log::flushLogThread()
{
    Log::instance()->asyncWrite();
}


// 真正的写入函数
void Log::write(int level, const char* format, ...)
{
    struct timeval now{0, 0
    };

    gettimeofday(&now, nullptr);
    time_t t_sec = now.tv_sec;
    struct tm *sysTime = localtime(&t_sec);
    struct tm t = *sysTime;

    va_list vaList;

    // 根据日志日志和日志行数来决定是否新建日志文件(不是同一天或日志的行数超过指定值)
    if (today != t.tm_mday || (lineCount  && (lineCount % LOG_MAX_LINE) == 0))
    {
        std::unique_lock<std::mutex> locker(mtx);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon, t.tm_mday);


        // 日期不同
        if (today != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", log_path, tail, log_suffix);
            today = t.tm_mday;
            lineCount = 0;
        }
        // 日志的行数超过指定值
        else 
        {
            int nTimes = lineCount / LOG_MAX_LINE;
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", log_path,
                     tail, nTimes, log_suffix);
        }

        locker.lock();
        flush();
        fclose(fp);

        fp = fopen(newFile, "a");
        assert(nullptr != fp);
    }

    {
        std::unique_lock<std::mutex> locker(mtx);
        ++lineCount;
        int n = snprintf(buff.beginWrite(), 128, "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);

        buff.updateWritPos(n);
        appendLogLevelTitle(level);

        va_start(vaList, format);
        int m = vsnprintf(buff.beginWrite(), buff.writableBytes(), format, vaList);

        buff.updateWritPos(m);
        buff.append("\n\0", 2);

        if(isAsync && logDeque_ptr && !logDeque_ptr->full())
            logDeque_ptr->push_back(buff.retrieveAllToStr());
        else
            fputs(buff.peek(), fp);

        buff.retrieveAll();
    }
}

void Log::flush()
{
    if (isAsync)
    {
        logDeque_ptr->flush();
    }
    fflush(fp);
}

// 获取日志级别
int Log::getLevel()
{
    std::lock_guard<std::mutex> locker(mtx);
    return log_level;
}

// 设置日志级别
void Log::setLevel(int level)
{
    lock_guard<mutex> locker(mtx);
    log_level = level;
}

bool inline Log::isOpen()
{
    return log_isOpen;
}