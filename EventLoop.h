#pragma once

#include<vector>
#include<atomic>
#include<memory>
#include<mutex>
#include<functional>

#include"Timestamp.h"
#include"CurrentThread.h"
#include"noncopyable.h"

class Poller;
class Channel;

class EventLoop:noncopyable
{
public:
    using Functor=std::function<void()>;

    EventLoop();
    ~EventLoop();

    //开启事件循环 
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime()const{return pollReturnTime_;}

    //在当前loop中执行cb
    void runInLoop(Functor cb);

    //将cb放入队列中，然后唤醒loop所在的线程，然后执行cb
    void queueInLoop(Functor cb);
    //唤醒loop所在的线程
    void wakeup();

    //channel相关操作
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    //判断channel是否存在
    bool hasChannel(Channel* channel);

    //判断当前loop是否在自己的线程里
    bool isInLoopThread()const {return threadId_==CurrentThread::tid();}

private:
    //唤醒的操作
    void handleRead();
    //执行回调操作
    void doPendingFunctors();

private:
    //存放channel的集合
    using ChannelList=std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_; //标识退出loop

    const pid_t threadId_;//记录当前loop所在线程的id
    
    Timestamp pollReturnTime_;//poller中发生事件的channel的时间
    std::unique_ptr<Poller>poller_;

    //主要作用，当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    int wakeupFd_;
    std::unique_ptr<Channel>wakeupChannel_;

    ChannelList activeChannels_;
    
    std::atomic_bool callingPendingFunctions_;//标识着是否进行回调函数的操作
    std::vector<Functor>pendingFunctors_;//存放loop的回调函数
    std::mutex mutex_;  //用来保护上述容器的线程安全问题

};