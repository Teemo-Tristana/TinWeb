
#include "./httpconn.h"

const char* HttpConn::hc_srcDir=nullptr;
std::atomic<int> HttpConn::userCount = 0;
bool HttpConn::isET = false;

HttpConn::HttpConn():hc_fd(-1), hc_addr({0}), hc_isClose(true){}

HttpConn::~HttpConn()
{
    closeFd();
}

void HttpConn::init(int sockFd, const sockaddr_in& addr)
{
    assert(sockFd > 0);
    ++userCount;
    hc_addr = addr;
    hc_fd = sockFd;
    writeBuff.retrieveAll();
    readBuff.retrieveAll();

    hc_isClose = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", hc_fd,getIP(), getPort(), static_cast<int>(userCount));
}


void HttpConn::closeFd()
{
    response.unmapFile();
    if (!hc_isClose)
    {
        hc_isClose = true;
        --userCount;
        close(hc_fd);
        LOG_INFO("Client[%d](%s:%d) close, userCount:%d", hc_fd,getIP(), getPort(), static_cast<int>(userCount));
    }
}


inline int HttpConn::getFd() const
{
    return hc_fd;
}

inline int HttpConn::getPort() const
{
    return static_cast<int>(hc_addr.sin_port);
}


inline const char* HttpConn::getIP() const 
{
    return inet_ntoa(hc_addr.sin_addr);
}

inline sockaddr_in HttpConn::getAddr() const{
    return hc_addr;
}


inline int HttpConn::toWriteBytes()
{
    return iov[0].iov_len + iov[1].iov_len;
}

inline bool HttpConn::isKeepAlive() const{
    return request.isKeepAlive();
}

ssize_t HttpConn::read(int* saveErrno)
{
    ssize_t len = -1;
    do
    {
        len = readBuff.readFd(hc_fd, saveErrno);
        if (len < 0)
            break;
    } while (isET);
    
    return len;
}

ssize_t HttpConn::write(int* saveErrno)
{
    ssize_t len = -1;

    do
    {
        len = writev(hc_fd, iov, hc_iovCnt);
        if (len < 0)
        {
            *saveErrno = errno;
            break;
        }

        if ((iov[0].iov_len + iov[1].iov_len) == 0)
            break;
        else if (static_cast<size_t>(len) > iov[0].iov_len)
        {
            iov[1].iov_base = (uint8_t*) iov[1].iov_base + (len - iov[0].iov_len);
            iov[1].iov_len  -= (len- iov[0].iov_len);

            if (iov[0].iov_len)
            {
                writeBuff.retrieveAll();
                iov[0].iov_len = 0;
            }
        }
        else 
        {
            iov[0].iov_base = (uint8_t*)iov[0].iov_base + len;
            iov[0].iov_len -= len;
            writeBuff.updateReadPos(len);
        }
    }while (isET || toWriteBytes() > 102400);
    return len;
}