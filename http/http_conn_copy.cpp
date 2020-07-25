/*
 * @Author: hancheng 
 * @Date: 2020-07-12 09:54:18 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-14 11:06:34
 */

/*
> 在HTTP报文中，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n,因此可以通过查看\r\n将报文头拆解为单独的行
进行行解析

> GET和POST请求报文的区别之一是有无消息体部分，GET请求没有消息体，当解析完空行之后，便完成了报文的解析。
> 后续的登录和注册功能，为了避免将用户名和密码直接暴露在URL中，我们在项目中改用了POST请求，将用户名和密码添加在报文中作为消息体进行了封装
  为此，我们需要在解析报文的部分添加解析消息体的模块。

循环体写成这样的原因: 
while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))

> 在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。
> 但在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。
> 解析完消息体后，报文的完整解析就完成了，但此时主状态机的状态还是CHECK_STATE_CONTENT，也就是说，符合循环入口条件，还会再次进入循环，这并不是我们所希望的。 
  为此，增加了该语句，并在完成消息体解析后，将line_status变量更改为LINE_OPEN，此时可以跳出循环，完成报文解析任务。
见(https://mp.weixin.qq.com/s?__biz=MzAxNzU2MzcwMw==&mid=2649274278&idx=7&sn=d1ab62872c3ddac765d2d80bbebfb0dd&scene=19#wechat_redirect)

知识点(详细见node.md): 
    > I/O复用: Linux下I/O复用系统调用主要有select,poll和epoll 我们这里只用到了epoll
    > HTTP格式
    > 有限状态机
1. 浏览器发出http请求, 主线程创建http对象并接收请求,将请求数据放入对应buffer中
    ,将该对象插入任务队列,工作线程从任务队列中取出一个任务进行处理

2.
*/
#include "http_conn.h"
#include "../log/log.h"
#include <map>
#include <mysql/mysql.h>
#include <fstream>

//#define connfdET //边缘触发非阻塞
#define connfdLT //水平触发阻塞

//定义HTTP响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

//当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
//网站根目录
const char *doc_root = "/home/ni/code/cpp/raw/myTinyWeb/root";
//将表中的用户名和密码放入map
map<string, string> users;
locker m_lock;

/****************************************epollf模块:epoll相关函数****************************************/
//将文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//往内核事件表注册事件
//将文件描述符fd上的EPOLLIN和EPOLLET事件注册到epollfd指示的epoll内核事件表中,参数one_shot指定是否对fd开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
/*EPOLLRDHUP 表示读端关闭:
    1.对端发送FIN(对端使用close或者shutdown(SHUT_WR))
    2.本端调用shutdown(SHUT_RD), 关闭 SHUT_RD 的场景很少
*/
#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    /*epoll 即使使用ET模式，一个socket上的某个事件还是可能被触发多次，
        采用线程城池的方式来处理事件，可能一个socket同时被多个线程处理
        如果对描述符socket注册了EPOLLONESHOT事件，那么操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次
        想要下次再触发则必须使用epoll_ctl重置该描述符上注册的事件，包括EPOLLONESHOT 事件。
        EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，
        需要再次把这个socket加入到EPOLL队列里 
*/

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核事件表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/******************************************************************************************************/

/*******************************************http模块:核心模块********************************************/

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

/*********************/

//关闭连接
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; //关闭一个连接，客户总量减一
    }
}

//初始化新接受的连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    //避免TIME_WAIT状态,用于调试
    //int reuse=1;
    //setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

//初始化连接(也就是初始化类中各成员变量)
void http_conn::init()
{
    mysql = nullptr;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET; //Method::GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机, 用于解析行内容,分析是请求报文中的哪一部分(见书P138)
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp = '\0';
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx]; //获取当前要分析的字节
        if (temp == '\r')                 //如果当前字节是'\r'(回车符),说明可能读取到一个完整的行
        {
            if ((m_checked_idx + 1) == m_read_idx) //是最后一个字符,则需进一步读取才能确认
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n') //下一个字符是'\n',则成功读取到一行
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD; //否在语法客户端发送点HTTP语法有误
        }
        else if (temp == '\n') //如果当前字节是'\n'(换行),说明可能读取到一个完整的行
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN; //所有内容都分析完毕也没遇到'\r',则返回LINE_OPEN,表示还需要继续读取分析
}

//循环读取客户数据，直到无数据可读或对方关闭连接(数据读取到m_read_buffer,同时更新m_read_idx)
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

#ifdef connfdLT //水平触发模式

    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;

    if (bytes_read <= 0)
    {
        return false;
    }

    return true;

#endif

#ifdef connfdET //边缘触发模式
    while (true)
    {
        //从套接字接收数据,存储在m_read_buf缓冲区
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            //非阻塞ET模式下,需要一次性将数据读完
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read; //修改m_read_idx点读取字节数
    }
    return true;
#endif
}

//主状态机解析报文中的请求行数据: 获取请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本
    //，其中各个部分之间通过\t或空格分隔。
    m_url = strpbrk(text, " \t");//返回请求行中空格或\t的位置(第一个匹配的位置)
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';//将该位置改为'\0', 用于将前面数据取出

    //取出数据,并通过与GET,POST方法进行比较,以确认请求方法
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET; //Method::GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST; //Method::POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    
    //m_url此时跳过了第一个空格或\t,但不知道哦之后是否还有
    //因此,将m_url向后偏移,通过查找,继续跳过空格或\t字符, 只想请求资源的第一个字符
    m_url += strspn(m_url, " \t"); //检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    
    //使用相同的逻辑判断HTTP版本号
    m_version = strpbrk(m_url, " \t");//检索字符串 str1 中第一个匹配字符串 str2 中字符的字符
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    //只支持HTTP/1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    
    //对请求资源的前7个自负进行判断,主要是因为有些报文的请求资源中会带有http://,因此需要单独处理
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/'); //查找字符串中第一个出现的指定字符
    }

    //同样对https判断
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    //一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    //当url为/时，显示欢迎界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    //HTTP请求行处理完毕,状态转移到头部字段分析(状态转移) 
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//分析头部字段
/*在报文中，请求头和空行的处理使用的同一个函数，这里通过判断当前的text首位是不是\0字符，若是，则表示当前处理的是空行，若不是，则表示当前处理的是请求头。*/
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0') //遇到一个空行,说明得到一个正确的HTTP请求
    {
        //判断m_content_length是否是0
        if (m_content_length != 0)  //不是,表明是POST请求
        {
            m_check_state = CHECK_STATE_CONTENT; //跳转消息体处理状态
            return NO_REQUEST;
        }
        return GET_REQUEST; //否则,是GET请求,报文解析结束
    }
    else if (strncasecmp(text, "Connection:", 11) == 0) //判断connection字段,是keep-alive还是close,决定是长连接还是短链接
    {
        text += 11;
        text += strspn(text, " \t");//跳过空格或\t
        if (strcasecmp(text, "keep-alive") == 0)//长连接
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0) //读取post请求的消息体长度
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)//处理HHost字段
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        //printf("oop!unknow header: %s\n",text);
        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

//解析HTTP请求的消息体, 判断http请求是否被完整的读入了
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//将主从状态机进行封装,对报文的每一行进行循环处理
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK; //初始化从状态机的HTTP
    HTTP_CODE ret = NO_REQUEST;        //初始化HTPP请求结果
    char *text = 0;
    /*      
    判断条件:
        主状态机转移到CHECK_STATE_CONTENT，该条件涉及解析消息体
        从状态机转移到LINE_OK，该条件涉及解析请求行和请求头部
        两者为或关系，当条件为真则继续循环，否则退出
    
    循环体
    从状态机读取数据
    调用get_line函数，通过m_start_line将从状态机读取数据间接赋给text
    主状态机解析text
    */
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        //m_start_line每一个数据行在m_read_buf中起始位置, m_check_idx是从状态机在m_read_buf中点读取位置
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text); //解析请求行(http第一部分)
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text); //解析请求头部信息(http第二部分)
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST) //完整解析GET请求后,跳转到do_request()响应函数
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT://解析内容部分(主体)(http第四部分)
        {
            ret = parse_content(text); 
            if (ret == GET_REQUEST)    //完整解析POST请求后,跳转到报文do_request()响应函数
            {
                return do_request();
                line_status = LINE_OPEN;
                break;
            }
        default:
            return INTERNAL_ERROR;
        }
        }
        return NO_REQUEST;
    }
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //同步线程登录校验
        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {

                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush();
    return true;
}

bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

//HTTP请求点入口函数 : 由线程池的工作线程调用
void http_conn::process()
{
    HTTP_CODE read_ret = process_read(); //调用process_read()完成报文解析
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret); //调用process_write()完成报文响应
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}
