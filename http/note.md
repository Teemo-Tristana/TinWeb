

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





/*
//往内核事件表注册事件
//将文件描述符fd上的EPOLLIN和EPOLLET事件注册到epollfd指示的epoll内核事件表中,参数one_shot指定是否对fd开启EPOLLONESHOT
EPOLLRDHUP 表示读端关闭:
    1.对端发送FIN(对端使用close或者shutdown(SHUT_WR))
    2.本端调用shutdown(SHUT_RD), 关闭 SHUT_RD 的场景很少
 
 epoll 即使使用ET模式，一个socket上的某个事件还是可能被触发多次，
        采用线程城池的方式来处理事件，可能一个socket同时被多个线程处理
        如果对描述符socket注册了EPOLLONESHOT事件，那么操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次
        想要下次再触发则必须使用epoll_ctl重置该描述符上注册的事件，包括EPOLLONESHOT 事件。
        EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，
        需要再次把这个socket加入到EPOLL队列里 
*/