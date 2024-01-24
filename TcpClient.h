#pragma once
#include"TcpConnection.h"
#include"noncopyable.h"

#include<mutex>
#include<memory>
#include<atomic>
#include<string>
class Connector;
using ConnectorPtr=std::shared_ptr<Connector>;

class TcpClient:noncopyable
{
public:
    TcpClient(EventLoop* loop,const InetAddress& serverAddr,const std::string& nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection()const
    {
        std::unique_lock<std::mutex>lock(const_cast<std::mutex&>(mutex_));
        return connection_;
    }

    EventLoop* getLoop()const { return loop_;}

    void setConnectionCallback(ConnectionCallback cb)
    { connectionCallback_=std::move(cb); }


    void setMessageCallback(MessageCallback cb)
    { messageCallback_=std::move(cb);}

    void setWriteCompleteCallback(WriteCompleteCallback cb)
    { writeCompleteCallback_=std::move(cb);}
private:

    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr& conn);


    EventLoop* loop_;
    ConnectorPtr connector_;
    const std::string name_;

    //连接事件回调
    ConnectionCallback connectionCallback_;
    //读写事件回调
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    std::atomic_bool connect_;

    int nextConnId_;
    std::mutex mutex_;

    TcpConnectionPtr connection_;

};