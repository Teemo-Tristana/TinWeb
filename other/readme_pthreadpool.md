半同步/半反应堆线程池
================================
使用一个工作队列完全解除主线程与工作线程的耦合关系：
    主线程：往工作队列中插入任务
    工作线程通过 竞争 来取得任务并执行

> * 同步I/O 模拟 procator模式
> * 半同步/半反应堆
> * 线程池

> *  工作流程:
![threadpool.png](./files/threadpool.png)
>* 函数
    >>* 1. pthread_create(arg1, arg2, arg3, arg4) 创建线程
        >>>* arg3必须是一个指向静态函数的函数指针
        >>>* arg4是arg3指向的函数的参数P304  
    >>* 2.  thread_detahc() 将线程状态设置为detached
        >>>* 函数1创建的线程默认状态是joinable,如果一个线程运行结束但是没有被pthread_join[回收线程],则该线程的状态**类似进程的Zombie[僵尸]进程**(资源没有被回收),因此,一般线程会调用pthread_join()等待该线程结束,得到该线程的状态码,回收其资源[类似进程的wait和waitpid];但是pthread_join(thread_id)的调用者会被阻塞,在Web服务器中主线程为每个新来的连接创建一个子线程时,主线程并不希望被阻塞,此时便可以使用pthread_detach(thread_id)[非阻塞],将子线程状态设置为detached,**这样子线程结束后便可以自动释放所有源**.
    >>* sem_wait(): 以原子操作将信号值-1,如果信号值为0,则阻塞知道这个信号量为非零值.
    >>* sem_post(): 以原子操作将信号值+1,若信号值大于0且有sem_wait()等待的线程,则将以某种方式唤醒.
    >>*  pthread_mutex_lock() 加锁
    >>*  pthread_mutex_unlock() 解锁
>* other
    >>* std::list类
    >>* C++异常机制
    >>* cat /proc/sys/kernel/threads_max 查看用户最大可创建的线程数
    >>* 工作线程通过竞争来获取任务,这里的资源是互斥锁?那这些线程之间怎么竞争互斥锁的呢???????


    线程池由一个线程安全的队列，以及多个 worker 线程组成。
    可以有多个 producer 线程，它们负责提交任务给线程池。
    接收到新任务之后，线程池会唤醒某个 worker 线程，worker 线程醒来后会取出任务并执行。
