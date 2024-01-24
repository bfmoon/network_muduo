#pragma once
#include"noncopyable.h"
#include"Callbacks.h"
#include"InetAddress.h"
#include"EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include<functional>
#include<string>
#include<memory>
#include<atomic>
#include<unordered_map>

class Acceptor;
class EventLoop;
class EventLoopThreadPool;


class TcpServer:noncopyable
{
public:
    using ThreadInitCallback=std::function<void(EventLoop*)>;
    //设置选项，端口是否可以复用
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop,const InetAddress& listerAddr,const std::string& nameArg,Option option=kNoReusePort);
    ~TcpServer();
    const std::string& ipPort()const{return ipPort_;}
    const std::string& name()const {return name_;}
    EventLoop* getLoop()const {return loop_;}

    //设置开启线程的数量
    void setThreadNum(int numThreads);

    void setThreadInitCallback(const ThreadInitCallback& cb){threadInitCallback_=cb;}
    void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_=cb;}
    void setMessageCallback(const MessageCallback& cb){messageCallback_=cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_=cb;}

    void start();
private:
    void newConnection(int sockfd,const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);


    using ConnectionMap=std::unordered_map<std::string,TcpConnectionPtr>;

    EventLoop* loop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool>threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;

    //是否启动
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;
};