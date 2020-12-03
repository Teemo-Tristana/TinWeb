
#include <unistd.h>

#include "./server/webserver.h"

int main()
{
    // daemon(1,0);

    WebServer server(
        PORT, EPOLL_MODE, TIMEOUTMS, OPT_LINGER,                 // 端口 ET模式 timeoutMs 优雅退出
        MYSQLPORT, MYSQLUSE, MYSQLPASSWD, DBNAME,              // Mysql配置信息
        POOL_NUM, THREAD_NUM, LOG_MODE, LOG_LEVEL, LOG_QUEUE_SIZE // 其他信息
    );
    
    server.start();
}