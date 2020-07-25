仿照TinyWebserver写一个轻量级的Web服务器

Raw_version文档
===============
Linux下C++轻量级Web服务器，助力初学者快速实践网络编程，搭建属于自己的服务器.

* 使用**线程池 + epoll(ET和LT均实现) + 模拟Proactor模式**的并发模型
* 使用**状态机**解析HTTP请求报文，支持解析**GET和POST**请求
* 通过访问服务器数据库实现web端用户**注册、登录**功能，可以请求服务器**图片和视频文件**
* 实现**同步/异步日志系统**，记录服务器运行状态
* 经Webbench压力测试可以实现**上万的并发连接**数据交换

基础测试
------------
* 服务器测试环境
	* Ubuntu版本16.04
	* MySQL版本5.7.29
* 浏览器测试环境
	* Chrome
	* FireFox

* 提前安装MySQL数据库和redis
* 终端打开mysql
    ```
        mysql  -u root -p
    ```

* util 中的 create 指定数据库名和表名即可
```C++
    //提前安装mysql
    string dbname =  "tinywebdb";//"tinywebdb";
    string tbname = "userinfo";
```
* 测试前确认已安装MySQL数据库

* 修改main.c中的数据库初始化信息

    ```C++
    // root root修改为服务器数据库的登录名和密码
	// qgydb修改为上述创建的yourdb库名
    connPool->init("localhost", "root", "root", "yourdb", 3306, 8);
    ```

* 修改http_conn.cpp中的root路径

    ```C++
	// 修改为root文件夹所在路径
    const char* doc_root="/home/qgy/TinyWebServer/root";
    ```

* 生成server

    ```C++
    make server
    ```

* 启动server

    ```C++
    ./server port
    ```

* 浏览器端

    ```C++
    ip:port
    ```

个性化测试
------

> * I/O复用方式，listenfd和connfd可以使用不同的触发模式，代码中使用LT + LT模式，可以自由修改与搭配.

- [x] LT + LT模式
	* listenfd触发模式，关闭main.c中listenfdET，打开listenfdLT
	    
	    ```C++
	    26 //#define listenfdET       //边缘触发非阻塞
	    27 #define listenfdLT         //水平触发阻塞
	    ```
	
	* connfd触发模式，关闭http_conn.cpp中connfdET，打开connfdLT
	    
	    ```C++
	    7 //#define connfdET       //边缘触发非阻塞
	    8 #define connfdLT         //水平触发阻塞
	    ```

- [ ] LT + ET模式
	* listenfd触发模式，打开main.c中listenfdET，关闭listenfdLT
	    
	    ```C++
	    26 //#define listenfdET       //边缘触发非阻塞
	    27 #define listenfdLT         //水平触发阻塞
	    ```
	
	* connfd触发模式，打开http_conn.cpp中connfdET，关闭connfdLT
	    
	    ```C++
	    7 #define connfdET       //边缘触发非阻塞
	    8 //#define connfdLT         //水平触发阻塞
	    ```

> * 日志写入方式，代码中使用同步日志，可以修改为异步写入.

- [x] 同步写入日志
	* 关闭main.c中ASYNLOG，打开同步写入SYNLOG
	    
	    ```C++
	    25 #define SYNLOG //同步写日志
	    26 //#define ASYNLOG   /异步写日志
	    ```

- [ ] 异步写入日志
	* 关闭main.c中SYNLOG，打开异步写入ASYNLOG
	    
	    ```C++
	    25 //#define SYNLOG //同步写日志
	    26 #define ASYNLOG   /异步写日志
	    ```
* 选择I/O复用方式或日志写入方式后，按照前述生成server，启动server，即可进行测试.



MyTinyWebServer:一个Linux下C++轻量级Web服务器
============================================
+ 组成:分为8个模块
    + lock(线程同步)模块[见lock文件夹]

    + 数据库模块：
        - mysql连接池模块[见CGImysql文件夹]
        - redis缓存模块[见userdata文件夹]

    + 定时器[见timer文件夹]
        - 基于升序链表的定时器
        - 基于时间轮的定时器
        - 基于时间堆的定时器

    + pthreadpool(线程池)模块[见threadpool文件夹]

    + http(http解析和响应)模块[见http文件夹]
        + I/O复用（select|poll|epoll）
        + 设计模式
        + 报文解析与响应
        
    + 日志系统[见log文件夹]
        + 同步日志
        + 异步日志

    + 测压模块[test_presure]
        - 这是一个第三方测试工具
    
    + root模块[将root文件夹]
        + 放置文件
        + 页面(html)

    + main模块[将main文件夹]



```
webbench -c 500 -t 5 http://127.0.0.1:5678/6
500 clients, running 5 sec.
```

+ 版本v1：时间轮定时器 + redis
```
Speed=944496 pages/min, 1763059 bytes/sec.
Requests: 78708 susceed, 0 failed
```

+ 版本v2：时间轮定时器 + mysql
```
Speed=1134156 pages/min, 2117068 bytes/sec.
Requests: 94513 susceed, 0 failed.
```


+ 社长版：链表定时器 + myql
```
Speed=1123788 pages/min, 2097715 bytes/sec.
Requests: 93649 susceed, 0 failed.
```