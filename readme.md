参考社长的项目和游双老师的<<Linux高性能服务器编程>>实现的轻量级服务器 TingyWeb 项目

Raw_version文档
===============
Linux下C++用来学习的Web服务器，结合之前的学习，搭建轻量级服务器。
* 基于BS模型，，采用epoll边沿触发模式(ET)来实现IO复用，使用同步IO模拟Proactor事件处理模式的并发模型
* 使用状态机解析http报文：主状态机内部调用从状态机，从状态机驱动主状态机。
* 采用基于时间轮的定时器定期处理**非活跃连接**
* 支持用户**注册，登录**功能，支持post和get方法
* 实现**日志系统**[同步/异步]记录服务器运行状态，
* 经Webbench压力测试可以实现**上万的并发连接**数据交换

基础测试
------------
* 服务器测试环境
	* Ubuntu版本16.04
	* MySQL 8.0.21 版
* 浏览器测试环境 
	* Chrome
	* FireFox

* 提前安装MySQL数据库
* 采用数据库保存用户和密码，因此需要提前安装mysql[本项目中使用的Mysql 8.0.21 ]

* util 中的 create 指定数据库名和表名即可【可以指定也可以使用默认的方式】
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
    //eg: ./server 12345
    ```

* 浏览器端访问：

    ```C++
    ip:port
    //eg：http://49.234.91.121:12345
    ```
MyTinyWebServer:一个Linux下C++轻量级Web服务器
============================================
+ 组成:分为8个模块
    + lock(线程同步)模块[见lock文件夹]

    + 数据库模块：
        - mysql连接池模块[见CGImysql文件夹]

    + 定时器[见timer文件夹]
        - 基于时间轮的定时器

    + pthreadpool(线程池)模块[见threadpool文件夹]

    + http(http解析和响应)模块[见http文件夹]
        + http解析类：使用有限状态机的设计思想，封装了http连接处理类，以实现解析报文请求和发送。[这里只有主状态机和从状态机]
    + 日志系统[见log文件夹]
        + 同步日志
        + 异步日志

    + 测压模块[test_presure]
        - 这是一个第三方测试工具
    
    + root模块[将root文件夹]
        + 文件
        + 页面(html)

    + main模块[将main文件夹]


```
webbench -c 500 -t 5 http://127.0.0.1:5678/6
500 clients, running 5 sec.
```

+ 时间轮定时器 + mysql
```
Speed=1134156 pages/min, 2117068 bytes/sec.
Requests: 94513 susceed, 0 failed.
```
🔗 TinyWeb服务器地址： http://49.234.91.121:12345/
欢迎访问
