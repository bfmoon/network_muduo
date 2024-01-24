#pragma once
#include"noncopyable.h"

#include<functional>
#include<string>
#include<vector>
#include<memory>

class EventLoop;
class EventLoopThread;

//EventLoop事件线程池类
class EventLoopThreadPool:noncopyable
{
public:
    using ThreadInitCall=std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop,const std::string& nameArg);
    ~EventLoopThreadPool();

    //设置开启线程的数量
    void setThreadNum(int numThreads){numThreads_=numThreads;}
    void start(const ThreadInitCall& cb=ThreadInitCall());

    EventLoop* getNextLoop();
    //获取所有事件
    std::vector<EventLoop*>getAllLoops();

    bool started()const{ return started_;}
    const std::string& name()const{return name_;}


private:
    EventLoop* baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;

    std::vector<std::unique_ptr<EventLoopThread>>threads_;
    std::vector<EventLoop*>loops_;
};