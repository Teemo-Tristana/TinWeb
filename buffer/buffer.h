#ifndef BUFFER_H_
#define BUFFER_H_
/**
 * 利用标准库的vector把char类型封装成Buffer类, 从而实现缓冲区的自动增长
 * 
 * 
 * 涉及知识点：
 *    vecotr的操作，vector的resize和reserve差别
 *    复制 ： std::copy()
 *    原子操作 ： std::atomice()
 *    重载、重写
 *    C++ 类型装换
 *    C++ 默认函数(默认析构函数)
 *    read 与 write
 *    I/O 向量：  struct iovec
 *    错误信息 errno
 *    
*/

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <assert.h>

const int READSIZE = 65535;
// Buffer 类
class Buffer 
{
    private:
        std::vector<char> buffer;   // 利用 vector 封装 char类型，实现自动增长
        std::atomic<std::size_t> al_read_pos; // 已读位置（原子操作）
        std::atomic<std::size_t> al_write_pos; // 已写位置（原子操作）


        char *beginPtr(); 
        const char *beginPtr() const; 

        void makeSpace(size_t len); 

    public:
        Buffer(size_t initBufferSize = 1024); 
        ~Buffer() = default;  

        size_t writableBytes() const;
        size_t readableBytes() const;
        size_t prePendableBytes() const; 

        const char *peek() const; 
        void ensuereWritable(size_t len); 
        void updateWritPos(size_t len);

        void updateReadPos(size_t len);
        void retrieveUntil(const char *end);

        void retrieveAll(); 
        std::string retrieveAllToStr();


        char *beginWrite();
        const char *beginWriteConst() const;
  

        void append(const char *str, size_t len);
        void append(const std::string &str);
        void append(const void *data, size_t len);
        void append(const Buffer &buffer);

        ssize_t readFd(int fd, int *saveErrno);
        ssize_t writFd(int fd, int *saveErrno);
};

#endif