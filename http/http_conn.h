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
        主状态机内部调用从状态机，从状态机驱动主状态机
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
    enum MAIN_STATUS
    {
        CHECK_STATE_REQUESTLINE = 0, //当前正在分析请求行
        CHECK_STATE_HEADER,          //当前正在分析头部字段
        CHECK_STATE_CONTENT          //当前正在分析内容字段
    };

    //从状态机状态
    enum CONG_STATUS
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
    static int m_epollfd;    // 所有socket上的事件都被注册带同一个epoll内核事件表中，因此将epoll文件描述符设置静态点
    static int m_user_count; //用户数量
    MYSQL *mysql;

private:
    int m_sockfd; // 该HTTP的连接socket和对端地址
    sockaddr_in m_address;

    /*以下是解析请求报文中的对应的6个变量,存储读取文的名称*/
    char m_read_buf[READ_BUFFER_SIZE];   //读缓冲区
    int alread_read_idx;                 //标识读缓冲区中已读入客户数据的最后一个字节的下一个位置
    int cur_checked_idx;                 //当前正在分析的字符在读缓冲区的位置
    int cur_start_line;                  //当前正在解析行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; //写缓冲区
    int need_write_idx;                  //写缓冲区中待发点字节数

    MAIN_STATUS m_check_state; //主状态机当前所处状态
    METHOD m_method;           //请求方法

    //http请求相关分析
    char m_real_file[FILENAME_LEN]; //客户端请求的目标文件的完整路径 = root + url, roots是网站的根目录
    char *m_url;                    //目标文件名
    char *m_version;                //HTTP的协议版本号
    char *m_host;                   //主机名
    int m_content_length;           //HTTP请求的消息体的长度
    bool keepALive;                 //HTPP是否保持连接

    //这里采用mmap映射客户请求的目标文件
    char *m_file_address; //客户请求点目标文件被mmap到内存中点起始位置
    //目标文件状态,通过它可以判断文件是否存在,是否是目录,并获取文件大小等信息
    struct stat m_file_stat; //检查目标文件权限

    //在响应报文时，分为http头部和内容实体两部分，因此采用 writev分析写，所以定义这两个成员,
    struct iovec m_iv[2]; //io向量机制iovec
    int m_iv_count;       //被写内存块的数量

    int ispost;     //是否启用的POST
    char *m_string; //存储请求头数据

    int bytes_to_send;   //待发送的字节数
    int bytes_have_send; //已发送的字节数

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr); //初始化新接受的连接
    void close_conn(bool isClose = true);           //关闭连接
    void process();                                 //处理客户请求
    bool read_once(bool is_et = false);             //非阻塞读操作(读取浏览器发送过来的数据)
    bool write();                                   //非阻塞写操作(响应报文写入函数)

    //获取客户端socket地址
    sockaddr_in *get_address()
    {
        return &m_address;
    }

    void initmysql_result(connection_pool *connPool, string tbname = "userinfo");

private:
    void init(); //初始化连接

    HTTP_CODE process_read();          //解析HTTP请求报文(从m_read_buf中读取数据,并处理请求报文)
    bool process_write(HTTP_CODE ret); //填充HTTP应答(向m_write_buf中写入响应报文)

    /*以下函数被process_read调用以分析HTTP请求*/
    HTTP_CODE parse_request_line(char *text);                 //主状态机解析报文中的请求行数据
    HTTP_CODE parse_headers(char *text);                      //主状态机解析报文中的请求头部数据
    HTTP_CODE parse_content(char *text);                      //主状态机解析报文中的请求内容
    HTTP_CODE do_request();                                   //生成响应报文
    char *get_line() { return m_read_buf + cur_start_line; }; //get_line用于将指针往后偏移,指向未处理的字符, m_start_line是已解析的字符,
    CONG_STATUS parse_line();                                 //从状态机读取并分析一行

    /*以下函数被process_write()调用填充HTTP应答*/
    void unmap(); // 解除mmap的映射
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    bool cfd_LT();
    bool cfd_ET();
};

#endif
