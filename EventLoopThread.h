#pragma once
#include"noncopyable.h"
#include"Thread.h"

#include<functional>
#include<mutex>
#include<condition_variable>

class EventLoop;

class EventLoopThread:noncopyable
{
public:
    using ThreadInitCallback=std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb=ThreadInitCallback(),
                    const std::string& name=std::string());
    ~EventLoopThread();
    //启动事件循环
    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    std::mutex mutex_;

    bool exiting_;
    Thread thread_;
    //条件变量
    std::condition_variable cond_;
    ThreadInitCallback callback_;//回调函数

};