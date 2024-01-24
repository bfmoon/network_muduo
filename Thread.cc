#include"Thread.h"
#include"CurrentThread.h"

#include<semaphore.h>
//静态变量类外初始化
std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name )
                :started_(false)
                ,joined_(false)
                ,tid_(0)
                ,func_(std::move(func))
                ,name_(name)
{
    setDefaultName();
}

//析构函数主要释放线程的智能指针
Thread::~Thread()
{
    if(started_&& !joined_)
    {
        //将线程设置为分离状态
        thread_->detach();
    }
}

// 开启线程的函数
void Thread::start()
{
    started_=true;

    //利用系统提供的信号量
    sem_t sem;
    sem_init(&sem,false,0);

    //然后利用Lambda表达式开启线程
    //利用lambda创建一个线程，并执行相关操作
 thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该线程函数
        func_(); 
    }));

    sem_wait(&sem);

}
void Thread::join()
{
    joined_=true;
    thread_->join();
}

//获取线程名
void Thread::setDefaultName()
{
    int num=++numCreated_;
    if(name_.empty())
    {
        char buf[32]={0};
        snprintf(buf,sizeof buf,"Thread%d",num);
        name_=buf;
    }
}