main模块：
============================================
+ 组成：
	- main.c
	- Makefile
+  main.c: 服务器点主函数
	- 将各个模块组合协调一起工作
	+ 调用I/O复用(select|poll|epoll)[这里使用是**epoll**]监听各个事件[共5种事件]，并对各事件作相应的处理.
		+ 1. **新的连接请求**
			- 判断条件 ： sockfd == listenfd
			- 处理	： 接受请求,保存用户数据(socketaddr地址, connfd等), 然后将connfd注册到epoll的内核表中,监听**读事件**

		+ 2. **对端关闭**
			- 判断条件： events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)
			- 处理 ： 关闭该连接,并释放该连接的资源

		+ 3. **读事件 且 是信号事件**
			- 判断条件：(sockfd == pipefd[0]) && (events[i].events & EPOLLIN)
			- 处理 ： 对信号处理[信号函数处理逻辑在这里实现]

		+ 4. **读事件 且 是客户端发送请求报文**
			- 判断条件： events[i].events & EPOLLIN
			- 处理 : 将数据读入读缓冲区,然后该任务插入线程池的任务队列中,再由任务队列进行处理

		+ 5. **写事件**
			- 判断条件 ：events[i].events & EPOLLOUT
			- 我们的web服务器只有一种写事件,就是工作线程处理完请求报文且封装成响应报文放入写数据缓冲区
			- 处理 ： 调用函数将响应报文发送给客户端( 浏览器)
			




