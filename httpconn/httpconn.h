#ifndef HTTPCONN_H
#define HTTPCONN_H


#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <atomic>



#include "httprequest.h"
#include "httpresponse.h"
#include "../log/log.h"
#include "../mysqlpool/sqlconnRAII.h"
#include "../buffer/buffer.h"



class HttpConn{
    public:
        HttpConn();
        ~HttpConn();

        void init(int sockFd, const sockaddr_in& addr);

        void closeFd();


        int getFd() const;
       
        int getPort()const;
         
        const char* getIP() const;
       
        sockaddr_in getAddr()const;
       
        int toWriteBytes();

        ssize_t read(int* saveErrno);

        ssize_t write(int* saveErrno);

      
        bool process();
       
        bool isKeepAlive()const;

    public:
        static bool isET;
        static const char* hc_srcDir;
        static std::atomic<int> userCount;

    private:
        int hc_fd;
        struct sockaddr_in hc_addr;

        bool hc_isClose;

        int hc_iovCnt;
        struct iovec iov[2];

        Buffer readBuff;
        Buffer writeBuff;

        HttpRequest request;
        HttpResponse response;
    
};

#endif