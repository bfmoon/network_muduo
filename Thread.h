#pragma once

#include"noncopyable.h"

#include<functional>
#include<thread>
#include<memory>
#include<string>
#include<atomic>
//线程类，开启线程以及设置线程启动数量
class Thread:noncopyable
{
public:
    using ThreadFunc=std::function<void()>;
    
    explicit Thread(ThreadFunc,const std::string& name=std::string());
    ~Thread();

    //开启线程的函数
    void start();
    void join();

    bool started()const {return started_;}
    pid_t tid()const {return tid_;}
    const std::string& name()const {return name_;}

    //返回线程个数
    static int numCreated(){return numCreated_;}

private:
    void setDefaultName();

    bool started_;//线程是否启动
    bool joined_;
    std::shared_ptr<std::thread>thread_;//线程指针，利用智能指针进行管理
    pid_t tid_;//线程号
    ThreadFunc func_;//线程函数
    std::string name_;
    static std::atomic_int numCreated_;//静态变量，记录全局创建的个数
};