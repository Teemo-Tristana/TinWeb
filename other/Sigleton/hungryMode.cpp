/*
 * @Author: hancheng 
 * @Date: 2020-07-11 15:19:36 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-11 15:33:45
 */
/*
单例模式之饿汉模式: 
    1. 运行时就创建实例,需要时则调用
    2. 线程安全,因为始终只有一个实例
    3.以空间换时间
*/

#include <iostream>

class singleton
{
private:
    singleton()
    {
        std::cout << "构造函数" << std::endl;
    }
    ~singleton()
    {
        std::cout << "析构函数" << std::endl;
    }

    singleton(const singleton &rhs){};
    const singleton &operator=(const singleton &rhs){};

private:
    // 唯一单实例对象指针
    static singleton *s_p;

public:
    static singleton *getInstance()
    {
        return s_p;
    }

    static void deleteInstance()
    {
        if (s_p)
        {
            delete s_p;
            s_p = nullptr;
        }
    }

    void Print()
    {
        std::cout << "我的实例内存地址是:" << this << std::endl;
    }
};

//代码一运行就是初始化创建实例,保证始终只有一个,因此本身线程安全
//懒汉模式和饿汉模式区别就在于这里: 懒汉模式是需要是才创建,饿汉模式是一开始就创建了
singleton *singleton::s_p = new (std::nothrow) singleton;



/////////////////////////////////////////
void *test(void *threadid)
{
    // 主线程与子线程分离，两者相互不干涉，子线程结束同时子线程的资源自动回收
    pthread_detach(pthread_self());

    int tid = *((int *)threadid);

    std::cout << "Hi, 我是线程 ID:[" << tid << "]\n"
              << std::endl;
    singleton::getInstance()->Print();
    pthread_exit(NULL);
}

const int NUM_THREAD = 5;
int main()
{
    pthread_t threads[NUM_THREAD] = {0};
    int indexes[NUM_THREAD] = {0};

    int ret = 0;
    int i = 0;

    std::cout << "main() : 开始 ... " << std::endl;
    for (int i = 0; i < NUM_THREAD; ++i)
    {
        std::cout << "main() : 创建线程:[" << i << "]" << std::endl;
        indexes[i] = i;
        // ret = pthread_create(&threads[i], NULL, test, (void*)&(indexes[i]));
        ret = pthread_create(&threads[i], NULL, test, (void *)&(indexes[i]));
        if (ret)
        {
            std::cout << "Error:无法创建线程" << ret << std::endl;
            exit(-1);
        }
    }

    singleton::deleteInstance();
    std::cout << "main() : 结束! " << std::endl;

    return 0;
}