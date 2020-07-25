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



