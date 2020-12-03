
#include "webserver.h"


WebServer::WebServer(int port, int epollMode, int timeOutMs, bool optLinger,
                  int sqlPort,const char* sqlUser, const char* sqlPwd,
                  const char* dbName, int connPoolNum, int threadNum,
                  bool openLog, int logLevel, int logQueueSize
        ):w_port(port), w_openLinger(optLinger), w_timoutMs(timeOutMs),
          w_isClose(false), timer_ptr(new HeapTimer()), threadpool_ptr(new ThreadPool(threadNum)),
          epoller_ptr(new Epoller())
        {
            w_srcDir = getcwd(nullptr, 256);
            assert(nullptr != w_srcDir);
            const char resources[] = "/resources";
            strncat(w_srcDir, resources, strlen(resources) );

            HttpConn::userCount = 0;
            HttpConn::hc_srcDir = w_srcDir;

            SqlConnPool::instance()->("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

            iniEventMode(epollMode);
            if (!initSocket())
            {
                w_isClose = true;
            }

            if (openLog)
            {
                Log::instance()->init(logLevel, "./log", ".log", logQueueSize);
                if (w_isClose)
                {
                    LOG_ERROR("**************Server init error**************!");
                }
                else 
                {
                    LOG_INFO("**************Server init **************")
                    LOG_INFO("port:%d, openLinger: %s", w_port, optLinger ? "true":"false");
                    LOG_INFO("listen mode : %s, openConn mode : %s",
                            (w_listenEvent & EPOLLET ? "ET" : "LT"),
                            (w_connectEvent & EPOLLET? "ET" : "LT"));

                    LOG_INFO("LogSys level : %d", logLevel);
                    LOG_INFO("srcDir : %s", HttpConn::hc_srcDir);
                    LOG_INFO("sqlConnPool num : % d, threadPool num : %d", connPoolNum, threadNum);
                }
            }

        }


WebServer::~WebServer()
{
    close(w_listenFd);
    w_isClose = true;
    free(w_srcDir);
    SqlConnPool::instance()->closePool();
}


void WebServer::start()
{
    int timeMs = -1;
    if (!w_isClose)
    {
        LOG_INFO("*******************Server start*******************");
    }

    while (!w_isClose)
    {
        if (w_timoutMs > 0)
        {
            timeMs = timer_ptr->getNextTick();
        }

        int eventCnts = epoller_ptr->waitTime(timeMs);

        for(int i = 0; i < eventCnts; ++i)
        {
            int fd = epoller_ptr->getFdOfEvent(i);
            uint32_t events = epoller_ptr->getEvent(i);

            if (w_listenFd == fd)
                dealListen();
            else if (events &(EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                assert(users.count(fd) > 0);
                closeConn(&users[fd]);
            }
            else if (events & EPOLLIN)
            {
                assert(users.count(fd) > 0);
                dealRead(&users[fd]);
            }
            else if (events& EPOLLOUT)
            {
                assert(users.count(fd) > 0);
                dealWrite(&users[fd]);
            }
            else 
            {
                LOG_ERROR("unexpected event");
            }
        }
    }
}

/*****************************************************************/


int WebServer::setFdNonBlock(int fd)
{
    assert(fd >= MIN_FD && fd <= MAX_FD);
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

bool WebServer::initSocket()
{
    int ret = 0;
    struct sockaddr_in addr;
    if (w_port > MAX_FD || w_port < MIN_FD)
    {
        LOG_ERROR("port:%d error", w_port);    
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(w_port);

    
    struct linger optLinger = {0};

    if(w_openLinger)
    {
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    w_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (w_listenFd < 0)
    {
        LOG_ERROR("create socket error, port:%d", w_port)
        return false;
    }

    ret = setsockopt(w_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0)
    {
        close(w_listenFd);
        LOG_ERROR("init linger error!, port:%d", w_port);
        return false;
    }


    int optVal = 1;
    ret = setsockopt(w_listenFd, SOL_SOCKET, SO_REUSEADDR,(const void*)&optVal,sizeof(int));
    if (-1 == ret)
    {
        LOG_ERROR("set socket setsoctopt error!");
        close (w_listenFd);
        return false;
    }

    ret = bind(w_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0)
    {
        LOG_ERROR("bind port:%d error",w_port);
        close(w_listenFd);
        return false;
    }

    ret = listen(w_listenFd, 5);
    if (ret < 0 )
     if (ret < 0)
    {
        LOG_ERROR("listn port:%d error",w_port);
        close(w_listenFd);
        return false;
    }

    ret = epoller_ptr->addFd(w_listenFd | EPOLLIN);
    if (0 == ret)
    {
        LOG_ERROR("add listen error");
        close(w_listenFd);
        return false;
    }

    setFdNonBlock(w_listenFd);
    LOG_INFO("server init success");
    return true;
    
}

void WebServer::iniEventMode(int epollMode)
{
    w_listenFd = EPOLLHUP; 
    w_listenEvent = EPOLLONESHOT | EPOLLWAKEUP;
    
    switch(epollMode)
    {
        case 0:
            break;
        
        case 1:
            w_connectEvent |= EPOLLET;
            break;

        case 2:
            w_listenEvent |= EPOLLET;
            break;

        case 3:
            w_connectEvent |= EPOLLET;
            w_listenEvent |= EPOLLET;
            break;
        
        default:
            w_connectEvent |= EPOLLET;
            w_listenEvent |= EPOLLET;
            break;
    }

    HttpConn::isET = (w_connectEvent & EPOLLET);
}

void WebServer::addClient(int fd, sockaddr_in addr)
{
    assert(fd >= MIN_FD && fd <= MAX_FD);
    users[fd].init(fd, addr);
    if (w_timoutMs > 0 )
    {
        timer_ptr->add(fd, w_timoutMs, std::bind(&WebServer::closeConn, this, &users[fd]));
    }

    epoller_ptr->addFd(fd, EPOLLIN | w_connectEvent);
    setFdNonBlock(fd);
    LOG_INFO("client[%d] in!", users[fd].getFd());
}

void WebServer::sendError(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0)
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }

    close(fd);
}

void WebServer::extentTime(HttpConn* client)
{
    assert(nullptr != client);
    if (w_timoutMs >  0)
    {
        timer_ptr->adjust(client->getFd(), w_timoutMs);
    }
}

void WebServer::closeConn(HttpConn* client)
{
    assert(nullptr != client);
    int fd = client->getFd();
    LOG_INFO("client[%d] quit!", fd);
    epoller_ptr->deleteFd(fd);
    client->closeFd();
}


void WebServer::dealListen()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do{
        int fd = accept(w_listenFd, (struct sockaddr*)&addr, &len);
        if (fd <= 0 )
            return ;
        else if(HttpConn::userCount >= MAX_FD)
        {
            sendError(fd, "server busy");
            LOG_WARN("cliets is full");
            return ;
        }
        
        addClient(fd, addr);
    }while(w_listenEvent & EPOLLET);

}

void WebServer::onProcess(HttpConn* client)
{
    if (client->process())
    {
        epoller_ptr->modifyFd(client->getFd(), w_connectEvent | EPOLLOUT);
    }
    else 
    {
        epoller_ptr->modifyFd(client->getFd(), w_connectEvent | EPOLLIN);
    }
}

void WebServer::onWrite(HttpConn* client)
{
    assert(nullptr != client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    
    if (0 == client->toWriteBytes())
    {
        if (client->isKeepAlive())
        {
            onProcess(client);
            return ;
        }
    }
    else if (ret < 0)
    {
        if (EAGAIN == writeErrno)
        {
            epoller_ptr->modifyFd(client->getFd(), w_connectEvent | EPOLLOUT);
            return ;
        }
    }

    closeConn(client);
}

void WebServer::onRead(HttpConn* client)
{
    assert(nullptr != client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN)
    {
        closeConn(client);
        return;
    }

    onProcess(client);
}



void WebServer::dealWrite(HttpConn* client)
{
    assert(nullptr != client);
    extentTime(client);
    threadpool_ptr->addTask(std::bind(&WebServer::onWrite,this, client));
}

void WebServer::dealRead(HttpConn* client)
{
    assert(nullptr != client);
    extentTime(client);
    threadpool_ptr->addTask(std::bind(&WebServer::onRead, this, client));
}






