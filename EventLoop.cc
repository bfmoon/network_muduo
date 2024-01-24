#include"EventLoop.h"
#include"Logger.h"
#include"Poller.h"
#include"Channel.h"

#include<unistd.h>
#include<sys/eventfd.h>
#include<fcntl.h>
#include<memory>
#include<errno.h>

//定义当前的线程id，而__thread表示的就是local lock，只在当前线程可以更改
__thread EventLoop* t_loopInThisThread=nullptr;

//定义超时时间
const int kPollTimeMs=10000;

//全局函数创建eventfd
// 创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd=::eventfd(0,EFD_NONBLOCK| EFD_CLOEXEC);
    if(evtfd<0)
    {
        LOG_ERROR("Failed in eventfd");
    }
    return evtfd;
}

EventLoop::EventLoop()
        :looping_(false),
        quit_(false),
        callingPendingFunctions_(false),
        threadId_(CurrentThread::tid()),
        poller_(Poller::newDefaultPoller(this)),
        wakeupFd_(createEventfd()),
        wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created,%p in thread: %d",this,threadId_);
    //如果t_loopInThisThread非空，则表示其他loop在该线程中
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop: [%d],exists in this thread [%d]",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread=this;
    }
    // 设置wakeupfd的事件类型(读事件)以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    //析构时，将所有事件设置为无效
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread=nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_=true;
    quit_=false;
    LOG_INFO("EventLoop %p start looping",this);
    //没有退出
    while(!quit_)
    {
        //首先刚上来清空活跃的channel列表
        activeChannels_.clear();
        //然后通过poll来进行相应事件处理
        pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);

        for(Channel* channel:activeChannels_)
        {
            // Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
         // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept fd《=channel subloop
         * mainLoop 事先注册一个回调cb（需要subloop来执行）    wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */ 
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping",this);
    looping_=false;
}

// 退出事件循环
void EventLoop::quit()
{
    quit_=true;
    //就是如果不是在当前线程的loop就要通过wakeup将其唤醒
    if(!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    //如果loop在当前线程中，则执行cb
    if(isInLoopThread())
    {
        cb();
    }
    else
    {
        //否则就先放入队列中，等待唤醒
        queueInLoop(std::move(cb));
    }
}

// 将cb放入队列中，然后唤醒loop所在的线程，然后执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex>lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
     // 唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctions_) 
    {
        wakeup(); // 唤醒loop所在线程
    }
}
// 唤醒loop所在的线程
//通过设置一个读事件
void EventLoop::wakeup()
{
    uint64_t one=1;
    ssize_t n=write(wakeupFd_,&one,sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes [%d] bytes instead of 8");
    }
}

// channel相关操作
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
// 判断channel是否存在
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

// 唤醒的操作,就是读取发送的数据
void EventLoop::handleRead()
{
    uint64_t one=1;
    ssize_t n=read(wakeupFd_,&one,sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() read [%d] bytes instead of 8");
    }
}
// 执行回调操作
void EventLoop::doPendingFunctors()
{
    std::vector<Functor>functors;
    callingPendingFunctions_=true;
    {
        std::unique_lock<std::mutex>lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for(const Functor& functor:functors)
    {
        functor();
    }
    callingPendingFunctions_=false;
}