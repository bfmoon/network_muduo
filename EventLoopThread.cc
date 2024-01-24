#include"EventLoopThread.h"
#include"EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback &cb ,
                const std::string &name )
        :loop_(nullptr)
        ,mutex_()
        ,exiting_(false)
        ,thread_(std::bind(&EventLoopThread::threadFunc,this),name)//绑定回调函数
        ,cond_()
        ,callback_(cb)
{}

//退出该线程的事件循环
EventLoopThread::~EventLoopThread()
{
    exiting_=true;
    if(loop_!=nullptr)
    {
        loop_->quit();
        //该事件退出后，设置该线程为分离状态
        thread_.join();
    }
}
// 启动事件循环
EventLoop *EventLoopThread::startLoop()
{
    //开启线程
    thread_.start();
    EventLoop* loop=nullptr;
    {
        std::unique_lock<std::mutex>lock(mutex_);
        //判断
        while(loop_==nullptr)
        {
            cond_.wait(lock);
        }
        loop=loop_;
    }
    return loop;
}

// 下面这个方法，是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    // 创建一个独立的eventloop，和上面的线程是一一对应的，one loop per thread
    EventLoop loop;
    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex>lock(mutex_);
        loop_=&loop;
        cond_.notify_one();
    }

    // EventLoop loop  => Poller.poll
    //然后执行eventLoop中的事件循环
    loop.loop();
    std::unique_lock<std::mutex>lock(mutex_);
    loop_=nullptr;
}