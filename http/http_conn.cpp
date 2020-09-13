/*
 * @Author: hancheng 
 * @Date: 2020-07-12 09:54:18 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-14 11:06:34
 */

#include <map>
#include <mysql/mysql.h>
#include <fstream>

#include "http_conn.h"
#include "../log/log.h"

using namespace std;

//#define connfdET       //边缘触发非阻塞
#define connfdLT //水平触发阻塞
#define oldwrite
// #define newwrite

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

const char *root = "/home/ubuntu16_04/learnGit/TinWeb/root"; //root目录下存放请求的资源和html文件
map<string, string> userInfo;                                //将数据库的用户名和密码载入到服务器的map中, key是用户名,value是密码
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

//从内核事件表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, bool is_et = false)
{
    epoll_event event;
    event.data.fd = fd;

    if (is_et)
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//往内核事件表注册事件
void addfd(int epollfd, int fd, bool is_oneshot, bool is_et = false)
{
    epoll_event event;
    event.data.fd = fd;
    if (is_et)
    {
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (is_oneshot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd); //对应ET模式，必须将fd设置为非阻塞，防止出现饥饿
}

/******************************************************************************************************/
//从连接池获取一个数据库连接，将已注册的用户数据载入 userInfo中
void http_conn::initmysql_result(connection_pool *connPool, string tbname)
{
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool); //先从连接池中取一个连接

    string sqlstring = "select username, passwd from " + tbname + ";";
    if (mysql_query(mysql, sqlstring.c_str()))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // int num_fields = mysql_num_fields(result);        //返回结果集中的列数
    MYSQL_RES *result = mysql_store_result(mysql);    //从表中检索完整的结果集
    MYSQL_FIELD *fields = mysql_fetch_fields(result); //返回所有字段结构的数组

    //将已注册的用户信息,载入到userInfo,方便后续判断用户是否已存在
    while (MYSQL_ROW row = mysql_fetch_row(result)) //将结果集的数据载入map中
    {
        userInfo[row[0]] = row[1];
    }
}

/*******************************************http模块:核心模块********************************************/

//类的静态变量，类中声明，类外初始化
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接socket
void http_conn::close_conn(bool isClose)
{
    if (isClose && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    //端口复用设置，调试用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    keepALive = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    cur_start_line = 0;
    cur_checked_idx = 0;
    alread_read_idx = 0;
    need_write_idx = 0;
    ispost = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

bool http_conn::cfd_LT()
{
    int bytes_read = recv(m_sockfd, m_read_buf + alread_read_idx, READ_BUFFER_SIZE - alread_read_idx, 0);
    alread_read_idx += bytes_read;
    if (bytes_read <= 0)
        return false;
    return true;
}

//非阻塞ET工作模式下，需要一次性将数据读完[循环读取客户数据，直到无数据可读或对方关闭连接]，因为 ET模式下只会触发一次
bool http_conn::cfd_ET()
{
    while (1)
    {
        int bytes_read = recv(m_sockfd, m_read_buf + alread_read_idx, READ_BUFFER_SIZE - alread_read_idx, 0);
        if (-1 == bytes_read)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (0 == bytes_read)
        {
            return false;
        }
        alread_read_idx += bytes_read;
    }
    return true;
}

//一次性读完数据
bool http_conn::read_once(bool is_et)
{
    if (alread_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    bool ret = false;
    if (is_et)
    {
        ret = cfd_ET();
    }
    else
    {
        ret = cfd_LT();
    }
    return ret;
}

/************************************process_read调用相关函数**************************************/
//解析http请求行：获取 url，method 和 http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        ispost = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER; //状态机跳转（从分析请求行 跳转到 分析请求头）
    return NO_REQUEST;
}

//从状态机， 用于解析出一行内容(http中一行是以\r\n为结束标志的)， \r或\n单独出现在一行中表示出错
http_conn::CONG_STATUS http_conn::parse_line()
{
    char temp = '0';
    for (; cur_checked_idx < alread_read_idx; ++cur_checked_idx)
    {
        temp = m_read_buf[cur_checked_idx];
        if (temp == '\r')
        {
            if ((cur_checked_idx + 1) == alread_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[cur_checked_idx + 1] == '\n')
            {
                m_read_buf[cur_checked_idx++] = '\0';
                m_read_buf[cur_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (cur_checked_idx > 1 && m_read_buf[cur_checked_idx - 1] == '\r')
            {
                m_read_buf[cur_checked_idx - 1] = '\0';
                m_read_buf[cur_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            keepALive = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
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

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (alread_read_idx >= (m_content_length + cur_checked_idx))
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
    CONG_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        cur_start_line = cur_checked_idx;
        LOG_INFO("%s", text);
        Log::get_instance()->flush();
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE: //解析请求行(http第一部分)
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER: //解析请求头部信息(http第二部分)
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request(); //完整解析GET请求后,跳转到do_request()响应函数
            }
            break;
        }
        case CHECK_STATE_CONTENT: //解析内容部分(主体)(http第四部分)
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST) //完整解析POST请求后,跳转到报文do_request()响应函数
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

/*当得到一个完整且正确的HTTP请求,就分析目标文件的属性,若目标文件存在,可对用户可读且不是目录
就使用mmap将其映射到内存地址m_file_address位置,然后告之调用者文件获取成功*/
http_conn::HTTP_CODE http_conn::do_request()
{

    //使用redis
    // redis_clt *m_redis = redis_clt::getinstance();
    strcpy(m_real_file, root);
    int len = strlen(root);
    //printf("m_url:%s\n", m_url);
    //找到 url 中 / 所在位置,进而判断 / 后的第一个字符
    const char *p = strrchr(m_url, '/'); //在m_url中找到/最后出现的位置

    //处理cgi: 登陆和注册校验
    if (ispost == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        /*********************/
        //将用户名和密码提取出来
        // user=hancheng&password=123 ??明文传输涉及到安全问题??
        char name[100], password[100];
        int i = 0;

        printf("m_string : %s\n", m_string);

        //以&为分隔符,前面是用户名,后面是密码
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';
        /*********************/

        /*
        //调试输出,真正允许时需要注释掉
        for (auto it = userInfo.begin(); it != userInfo.end(); ++it)
        {
            cout << "name:" << it->first << "\tpassword:" << it->second << endl;
        }

        printf("*(p+1) = %c\n", *(p + 1));

*/

        /*同步线程
        if (*(p + 1) == '3') //注册校验
        {
            //
            bool hasKey = m_redis->is_key_exist(name);
            if (hasKey) //已注册
            {
                cout << "在 redis 中已经存在关键字: " << name << endl;
                strcpy(m_url, "/registerError.html");
            }
            else //未注册
            {
                cout << "写入 redis 中\n";
                m_redis->setUserpasswd(name, password);
                strcpy(m_url, "/log.html");
            }
            //
        */
        printf("*(p+1) = %c\n", *(p + 1));

        if (*(p + 1) == '3')
        {
            //先检测数据库中是否有重名的,没有重名的，进行增加数据
            string value1(name);
            string vlaue2(password);
            string sqlstring = "insert into userinfo(username, passwd) values( '" + value1 + "', '" + vlaue2 + "');";

            char *sql_insert = const_cast<char *>(sqlstring.c_str());

            //判断是否已注册
            if (userInfo.find(name) == userInfo.end())
            {

                //向数据库插入数据时,需要通过锁来同步数据
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                userInfo.insert(pair<string, string>(name, password));
                m_lock.unlock();

                LOG_INFO("%s", sql_insert);

                if (!res) //校验成功,跳转到登陆页面
                    strcpy(m_url, "/log.html");
                else //校验失败,跳转到注册失败页面
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html"); //用户已存在
        }
        //登陆校验
        else if (*(p + 1) == '2')
        {
            /*
            // if (m_redis->getUserpasswd(name) == password)
            if (userInfo.find(name))
            {
                // cout <<"使用 redis 测试登录\n";
                strcpy(m_url, "/welcome.html");
            }
            else
            {
                strcpy(m_url, "/logError.html");
            }
            //

            */
            //直接判断浏览器端输入的用户名和密码在userInfo是否中可以查找到，返回1，否则返回0
            if (userInfo.find(name) != userInfo.end() && userInfo[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }

    if (*(p + 1) == '0') //'0' 注册页面
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '1') //'1' 登录页面
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5') // '5' 图片页面
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6') //'6' 视频页面
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        // strcpy(m_url_real, "/video0.html");
        strcpy(m_url_real, "/video01.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7') //请求资源为 /6 跳转到fans界面
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else //否则发送 url 实际请求的文件
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode)) //S_ISDIR(st_mode) //判断是否是一个目录
        return BAD_REQUEST;
    int fd = open(m_real_file, O_RDONLY);

    //通过mmap映射将文件映射到内存中
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); //关闭文件描述符避免资源的浪费和占用
    return FILE_REQUEST;
}

//释放mmap的内存映射
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

//写HTTP响应 : 将响应报文写入浏览器端
bool http_conn::write()
{
    int temp = 0;
    int addsum = 0;
    if (bytes_to_send == 0) //待发数据0,表示响应报文为空
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (true)
    {
        //将响应报文状态行, 头部消息, 空行, 响应正文发给浏览器
        temp = writev(m_sockfd, m_iv, m_iv_count);

#ifdef newwrite
        if (temp > 0) //正常发送, temp是writev发送的字节数
        {
            bytes_have_send += temp;              //更新已发送的字节数
            addsum = bytes_to_send - m_write_idx; //偏移文件iovec的指针
        }
        if (temp < 0)
        {
            /*如果缓冲区已满,则等待下一论EPOLLOUT事件,在此期间服务器无法立即接受
              同一客户的下一个请求,但是可保证连接的完整性*/
            if (errno == EAGAIN)
            {
                if (bytes_have_send >= m_iv[0].iov_len) //第一个iovec头部信息已经发送,发送第二个iovec数据
                {
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_file_address + addsum;
                    m_iv[1].iov_len = bytes_to_send;
                }
                else //否则继续发送第一个iovec头部消息数据
                {
                    m_iv[0].iov_base = m_write_buf + bytes_to_send;
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                }

                modfd(m_epollfd, m_sockfd, EPOLLOUT); //重新注册写事件
                return true;
            }
            unmap();
            return false;
        }

#endif

#ifdef oldwrite
        if (temp < 0)
        {
            /*如果缓冲区已满,则等待下一论EPOLLOUT事件,在此期间服务器无法立即接受
              同一客户的下一个请求,但是可保证连接的完整性*/
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT); //重新注册写事件
                return true;
            }
            //发送失败,但是不缓冲区问题,取消映射
            unmap();
            return false;
        }

        bytes_have_send += temp; //更新已发送字节
        bytes_to_send -= temp;   //偏移文件iovec的指针

        //第一个iovec头部信息已经发送,发送第二个iovec数据
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            //不在继续发送头部消息
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - need_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else //否则继续发送第一个iovec头部消息数据
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
#endif

        if (bytes_to_send <= 0) //判断数据是否发送完毕
        {
            unmap();                             //取消内存映射
            modfd(m_epollfd, m_sockfd, EPOLLIN); //在epoll内核表中重置EPOLLONESHOT

            if (keepALive) //若是长连接
            {
                init(); //重新初始化HTTP对象
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

/*以下函数被process_write()调用填充HTTP应答*/
//添加响应
bool http_conn::add_response(const char *format, ...)
{
    if (need_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + need_write_idx, WRITE_BUFFER_SIZE - 1 - need_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - need_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    need_write_idx += len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush();
    return true;
}

//添加状态行
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

//添加消息头
bool http_conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

//添加内容实体长度
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

//添加内容实体类型
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

//是否保持持久连接
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (keepALive == true) ? "keep-alive" : "close");
}

//添加空行
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

// 添加内容实体
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

/*服务器子线程调用process_write完成响应报文，随后注册epollout事件。
服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端。*/
//根据do_requset返回的状态, 服务器子线程调用process_write向m_write_buf中写入响应报文
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR: // 5xx 服务器错误
    {
        add_status_line(500, error_500_title); //添加状态行
        add_headers(strlen(error_500_form));   //添加消息报文头
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST: // 404 Not Found
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST: //403 Forbidden 无访问权限
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST: // 200 请求成功
    {
        add_status_line(200, ok_200_title); //添加状态行
        if (m_file_stat.st_size != 0)       //请求资源存在
        {
            add_headers(m_file_stat.st_size); //添加消息报文头

            //第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = need_write_idx;

            //第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;

            m_iv_count = 2;
            //发送的全部数据为响应报文头部信息和文件大小
            bytes_to_send = need_write_idx + m_file_stat.st_size;

            return true;
        }
        else
        {
            //如果请求的资源大小为0，则返回空白html文件
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    //除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = need_write_idx;
    m_iv_count = 1;
    bytes_to_send = need_write_idx;
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
