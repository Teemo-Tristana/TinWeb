/*
 * @Author: hancheng 
 * @Date: 2020-07-12 09:24:18 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-14 10:33:47
 */

/*
http模块:
    > 在类里面设计一种状态,且允许一个对象在其内部状态改变时改变它的行为
    > 有限状态机
    > http中两种状态:
        > 主状态机
        > 从状态机
*/

#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

#include "../userdata/redis.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;       //文件名最大长度
    static const int READ_BUFFER_SIZE = 2048;  //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024; //写缓冲区大小

    //http请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };

    //解析客户请求时,主状态机状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, //当前正在分析请求行
        CHECK_STATE_HEADER,          //当前正在分析头部字段
        CHECK_STATE_CONTENT          //当前正在分析内容字段
    };

    //从状态机状态
    enum LINE_STATUS
    {
        LINE_OK = 0, //读取一个完整行
        LINE_BAD,    //行出错
        LINE_OPEN    //读取的行不完整
    };

    //Http处理结果
    enum HTTP_CODE
    {
        NO_REQUEST,        //请求不完整,需要继续读取
        GET_REQUEST,       //获取一个完整的客户请求
        BAD_REQUEST,       //客户请求有语法错误
        NO_RESOURCE,       //请求资源不存在
        FORBIDDEN_REQUEST, //客户对资源没有足够点权限
        FILE_REQUEST,      //请求资源存在,可以正常访问
        INTERNAL_ERROR,    //服务器内部出错
        CLOSED_CONNECTION  //客户端链接已关闭
    };


public:
    // 所有socket上点事件都被注册带同一个epoll内核时间表中
    static int m_epollfd;    //因此将epoll文件描述符设置静态点
    static int m_user_count; //用户数量
    MYSQL *mysql;

private:
    int m_sockfd;          //http连接的socket
    sockaddr_in m_address; //客户端socket地址

    /*以下是解析请求报文中的对应的6个变量,存储读取文的名称*/
    char m_read_buf[READ_BUFFER_SIZE];   //读缓冲区
    int m_read_idx;                      //标识读缓冲区中已经读入的客户数据点最后一个字节的下一个位置
    int m_checked_idx;                   //当前正在分析的字符在读缓冲区的位置
    int m_start_line;                    //当前正在解析行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; //写缓冲区
    int m_write_idx;                     //写缓冲区中待发点字节数
    
    CHECK_STATE m_check_state;           //主状态机当前所处状态
    // Check_state m_check_state;
    METHOD m_method; //请求方法
    // Method m_method;
    
    /*以下是解析请求报文中对应点6个变量,存储读取文件点名称*/
    char m_real_file[FILENAME_LEN]; // 客户端请求目标文件点完整路径doc_root(网站根目录) + m_url
    char *m_url;                    //客户请求点目标文件名
    char *m_version;                //HTTP的协议版本号
    char *m_host;                   //主机名
    int m_content_length;           //HTTP请求的消息体的长度
    bool m_linger;                  //HTPP是否保持连接
  
    char *m_file_address;           //客户请求点目标文件被mmap到内存中点起始位置
    struct stat m_file_stat;        //目标文件点状态,通过它可以判断文件是否存在,是否是目录,并获取文件大小等信息
    
     //使用writev来执行写操作,所以定义这两个成员,
    struct iovec m_iv[2];           //io向量机制iovec
    int m_iv_count;                 //被写内存块的数量
    
    int cgi;                        //是否启用的POST
    char *m_string;                 //存储请求头数据
    
    int bytes_to_send;              //待发送的字节数
    int bytes_have_send;            //已发送的字节数

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化新接受的连接
    void init(int sockfd, const sockaddr_in &addr);

    //关闭连接
    void close_conn(bool real_close = true);

    //处理客户请求
    void process();

    //非阻塞读操作(读取浏览器发送过来的数据)
    bool read_once();

    //非阻塞写操作(响应报文写入函数)
    bool write();

    //获取客户端socket地址
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    void initmysql_result(connection_pool *connPool);

private:
    //初始化连接
    void init();
     //解析HTTP请求报文(从m_read_buf中读取数据,并处理请求报文)
    HTTP_CODE process_read();
    //填充HTTP应答(向m_write_buf中写入响应报文)
    bool process_write(HTTP_CODE ret); 

    /*以下函数被process_read调用以分析HTTP请求*/
    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机解析报文中的请求头部数据
    HTTP_CODE parse_headers(char *text);
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    //生成响应报文
    HTTP_CODE do_request();
    //get_line用于将指针往后偏移,指向未处理的字符, m_start_line是已解析的字符, 
    char *get_line() { return m_read_buf + m_start_line; };
    //从状态机读取一行,分析是请求报文中的哪一部分
    LINE_STATUS parse_line();

    /*以下函数被process_write()调用填充HTTP应答*/
    //根据响应报文格式,生成对应8个部分,以下函数均由do_request()调用,以填充HTTP应答
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
};

#endif
