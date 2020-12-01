#include "buffer.h"

Buffer::Buffer(size_t initBufferSize):buffer(initBufferSize), al_read_pos(0), al_write_pos(0)
{}

// 可写字节数
size_t inline Buffer::writableBytes() const
{
    return buffer.size() - al_write_pos;
}

// 可读字节数
size_t inline Buffer::readableBytes() const
{
    return al_write_pos - al_read_pos;
}

// 返回已经的最后一个位置
size_t inline Buffer::prePendableBytes() const 
{
    return al_read_pos;
}

// 可读的首地址
const char * Buffer::peek() const
{
    return beginPtr() + al_read_pos;
}

// 确保 buffer 空间剩余空间可写
void inline Buffer::ensuereWritable(size_t len)
{
    if (writableBytes() < len)
    {
        makeSpace(len);
    }
}

// 更新已写位置
void inline Buffer::updateWritPos(size_t len)
{
    al_write_pos += len;
}

// 更新已读位置
void inline Buffer::updateReadPos(size_t len)
{
    assert(len <= readableBytes());
    al_read_pos += len;
}

// 更新已读位置至指定位置
void Buffer::retrieveUntil(const char* end)
{
    assert(peek() <= end);
    updateReadPos(end - peek());
}

// 重置整个 buffer 
void inline Buffer::retrieveAll()
{
    bzero(&buffer[0], buffer.size());
    al_read_pos = 0;
    al_write_pos = 0;
}

// 将可读的char*转为string并重置buffer
std::string Buffer::retrieveAllToStr()
{
    std::string str(peek(), readableBytes());
    retrieveAll();
    return str;
}

// 返回最后一个已写位置
char*Buffer::beginWrite()
{
    return beginPtr() + al_write_pos;
}


// 返回最后一个已写位置(const 版本)
const char* Buffer::beginWriteConst() const 
{
    return beginPtr() + al_write_pos;
}

/**
 * 以下四个函数是 append() 重载版本
 * 将第一个参数的内容追加到 buffer 数组中
 */
// char* 版本
void Buffer::append(const char* str, size_t len)
{
    assert(str);
    ensuereWritable(len);
    std::copy(str, str + len, beginWrite());
    updateWritPos(len);
}

// string 版本
void Buffer::append(const std::string & str)
{
    append(str.data(), str.size());
}

// void* 版本
void Buffer::append(const void *data, size_t len)
{
    assert(data);
    append(static_cast<const char *>(data), len);
}

// Buffer 版本
void Buffer::append(const Buffer& buffer)
{
    append(buffer.peek(), buffer.readableBytes());
}


// 读取 Fd
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char temp_buff[READSIZE];
    
    const size_t writeable = writableBytes();
    // io 块
    struct iovec iov[2];
    // 分散读
    iov[0].iov_base = beginPtr() + al_write_pos;
    iov[0].iov_len = writeable;
    iov[1].iov_base = temp_buff;
    iov[1].iov_len = sizeof(temp_buff);

    // 从 fd 中读取到 iov 中
    const ssize_t len = readv(fd, iov, 2);

    /**
     * 首先获取从 fd 中读取的字节数 len
     * 若 len 小于 0,记录错误信息
     * 若 len 小于 可写字节数， 更新 已写位置
     * 否则，大于 可写字节数，则 更新 已有字节为buffer的大小，同时将buffer扩容
    */
    if (len < 0)
        *saveErrno = errno;
    else if (static_cast<size_t>(len) <= writeable)
        al_write_pos += len;
    else 
    {
        al_write_pos = buffer.size();
        append(temp_buff, len - writeable);
    }

    return len;
}


// 写入 Fd
ssize_t Buffer::writFd(int fd, int* saveErrno)
{
    size_t readSize = readableBytes();

    // 写入
    ssize_t len = write(fd, peek(), readSize);

    if (len < 0)
    {
        *saveErrno = errno;
        return len;
    }
    al_read_pos += len;
    return len;
}


// buffer 数组的起始地址
char* Buffer::beginPtr()
{
    return &(*buffer.begin());
}

// buffer 数组的起始地址 (常量版本)
const char* Buffer::beginPtr() const{
    return &(*buffer.begin());
}

// 申请空间（循环利用）
void Buffer::makeSpace(size_t len)
{
    if (writableBytes() + prePendableBytes() < len)
    {
        buffer.resize(al_write_pos + len + 1);
    }
    else 
    {
        size_t readAble = readableBytes();
        std::copy(beginPtr() + al_read_pos, beginPtr() + al_write_pos, beginPtr());
        al_read_pos = 0;
        al_write_pos = al_read_pos + readAble;
        assert(readAble == readableBytes());
    }
}