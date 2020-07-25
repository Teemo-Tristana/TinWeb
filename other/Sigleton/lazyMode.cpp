/*
 * @Author: hancheng 
 * @Date: 2020-07-11 14:29:45 
 * @Last Modified by: hancheng
 * @Last Modified time: 2020-07-11 15:18:42
 */
/*
单例模式之懒汉模式: 
    1. 运行类时,实例对象并不一定存在,只有当需要时才创建
    2. 存在线程安全问题: 因为可能创建多个实例
    3.解决方法:
        a. 加锁
        b.使用局部静态变量
    4.以时间换空间,适用于访问量小的情况
*/

#include <iostream>  // std::cout
#include <mutex>     // std::mutex
#include <pthread.h> // pthread_create

using namespace std;
//普通懒汉模式
/*存在线程安全问题  因为可能创建多个实例*/
class singleMode01
{
public:
    static singleMode01 *getInstance()
    {
        if (nullptr == s_p)
        {
            s_p = new (std::nothrow) singleMode01;
        }
        return s_p;
    };

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
        std::cout << " 我的实例内存地址是: " << this << std::endl;
    }

private:
    singleMode01()
    {
        std::cout << "构造函数 " << this << std::endl;
    }
    ~singleMode01()
    {
        std::cout << "析构函数 " << this << std::endl;
    }

private:
    static singleMode01 *s_p;
};

singleMode01 *singleMode01::s_p = nullptr;

//加锁实现懒汉模式线程安全
class singleMode02
{
public:
    static singleMode02 *getInstance()
    {
        //使用两个if判断语句的技术称为双检锁
        //好处:只有为空时才加锁避免每个调用getInstance()都加锁
        if (nullptr == s_p2)
        {
            unique_lock<mutex> lock(m_mutex);
            if (nullptr == s_p2)
            {
                s_p2 = new (std::nothrow) singleMode02;
            }
        }
        return s_p2;
    };

    static void deleteInstance()
    {
        unique_lock<mutex> lock(m_mutex);
        if (s_p2)
        {
            delete s_p2;
            s_p2 = nullptr;
        }
    }

    void Print()
    {
        std::cout << " 我的实例内存地址是: " << this << std::endl;
    }

private:
    singleMode02()
    {
        std::cout << "构造函数 " << this << std::endl;
    }

    ~singleMode02()
    {
        std::cout << "析构函数 " << this << std::endl;
    }

    singleMode02(const singleMode02 &s) {}
    const singleMode02 &operator=(const singleMode02 &s){};

private:
    static singleMode02 *s_p2;
    static std::mutex m_mutex;
};

singleMode02 *singleMode02::s_p2 = nullptr;
std::mutex singleMode02::m_mutex;

//用内部静态变量实现安全的懒汉单例模式
//在C++11内部静态变量的方式里是线程安全的，只创建了一次实例
//推荐方式
class singleMode03
{
public:
    static singleMode03 &getInstance()
    {
        // if (nullptr == s_p)
        // {
        //     s_p = new (std::nothrow) singleMode03;
        // }
        static singleMode03 s_p3;
        return s_p3;
    };

    // static void deleteInstance()
    // {
    //     if (s_p)
    //     {
    //         delete s_p;
    //         s_p = nullptr;
    //     }
    // }

    void Print()
    {
        std::cout << " 我的实例内存地址是: " << this << std::endl;
    }

private:
    singleMode03()
    {
        std::cout << "构造函数 " << this << std::endl;
    }
    ~singleMode03()
    {
        std::cout << "析构函数 " << this << std::endl;
    }

// private:
//     static singleMode03 *s_p;
};



void *test(void *threadid)
{
      // 主线程与子线程分离，两者相互不干涉，子线程结束同时子线程的资源自动回收
    pthread_detach(pthread_self());

    int tid = *((int *)threadid);

    std::cout << "Hi, 我是线程 ID:[" << tid << "]\n" << std::endl;
    // singleMode02::getInstance()->Print();
    singleMode03::getInstance().Print();
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

    // singleMode01::deleteInstance();
    std::cout << "main() : 结束! " << std::endl;

    return 0;
}



/****************************************************/
//1. 经典的懒汉模式: 使用双检锁

class single{
    private:
        static single* p;
        static pthread_mutex_t lock;

        single()
        {
            pthread_mutex_init(&lock, NULL);
        }
        
        ~single(){}
    public:
        //共有方法获取实例
        static single* getinstance();
};

pthread_mutex_t single::lock;
single* single::p = NULL;

//  使用双检锁
/* 若只检测一次,则每次调用获取实例的方法时,都要加锁,这会影响程序性能,
   使用双检索可以有效避免这种情况,只会在第一次创建单例时加锁,其他情况都不在
   符合 NULL == p的情况,直接返回已存在的实例
*/
single* single::getinstance()
{
    if (NULL == p)
    {
        pthread_mutex_lock(&lock);
        if (NULL == p)
        {
             p = new single;
        }
        pthread_mutex_unlock(&lock);
    }
    return p;
}


/****************************************************/
//2. 经典的懒汉模, 不是用锁,<<Effective C++>> (Item 04)
/* 使用函数中的局部静态对象, 可以避免加解锁,同时线程安全*/
class single{
    private :
        single(){}
        ~single(){}

    public:
        static single* getinstance();
};

single* single::getinstance()
{   
    static single obj;
    return &obj;
}

/****************************************************/