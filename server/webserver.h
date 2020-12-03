
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../epoller/epoller.h"
#include "../httpconn/httpconn.h"
#include "../threadpool/threadpool.h"
#include "../mysqlpool/sqlconnpool.h"
#include "../mysqlpool/sqlconnRAII.h"



// 端口
const int PORT = 5678;
// EPOLL 模式
const int EPOLL_MODE = 3;
// 连接时间
const int TIMEOUTMS = 60000; // 60 s = 60000 ms
// 退出方式
const bool OPT_LINGER = false;

// mysql 配置信息
const int MYSQLPORT = 3306;
const char *MYSQLUSE = "root";
const char *MYSQLPASSWD = "root";
const char *DBNAME = "tinywebdb";

// 连接池数量
const int POOL_NUM = 12;

// 线程池数量
const int THREAD_NUM = 8;

// 日志开关(同步or异步)
const bool LOG_MODE = true;

// 日志等级
const int LOG_LEVEL = 1;

// 异步日志队列容量
const int LOG_QUEUE_SIZE = 1024;


class WebServer{
    public:
        WebServer(int port, int epollMode, int timeOutMs, bool optLinger,
                  int sqlPort,const char* sqlUser, const char* sqlPwd,
                  const char* dbName, int connPoolNum, int threadNum,
                  bool openLog, int logLevel, int logQueueSize
        );

        ~WebServer();

        void start();

    private:
        
        static int setFdNonBlock(int fd);
                
        bool initSocket();

        void iniEventMode(int epollMode);


        void addClient(int fd, sockaddr_in addr);

        void sendError(int fd, const char* info);

        void extentTime(HttpConn* client);

        void closeConn(HttpConn* client);

        void dealListen();
        void onProcess(HttpConn* client);

        void onRead(HttpConn * client);
        void onWrite(HttpConn* client);

        
        void dealWrite(HttpConn* client);
        void dealRead(HttpConn* client);
       
    private:
        int w_port;
        int w_openLinger;
        int w_timoutMs;
        int w_listenFd;

        bool w_isClose;
        char* w_srcDir;

        uint32_t w_listenEvent;
        uint32_t w_connectEvent;

        static const int MAX_FD = 65535;
         static const int MIN_FD = 1024;

        std::unique_ptr<HeapTimer> timer_ptr;
        std::unique_ptr<ThreadPool> threadpool_ptr;

        std::unique_ptr<Epoller> epoller_ptr;

        std::unordered_map<int, HttpConn> users;

};


#endif