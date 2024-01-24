#pragma once
#include"noncopyable.h"
#include"InetAddress.h"

#include<functional>
#include<memory>
#include<atomic>

class Channel;
class EventLoop;

class Connector:noncopyable,
                public std::enable_shared_from_this<Connector>
{
public:
    using NewConnectionCallback=std::function<void (int sockfd)>;

    Connector(EventLoop* loop,const InetAddress& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {newConnectionCallback_=cb;}

    void start();
    void restart();
    void stop();

    const InetAddress& serverAddr()const { return serverAddr_;}

private:
    enum States { kDisconnected,kConnecting,kConnected};
    static const int kMaxRetryDelayMs=30*1000;
    static const int kInitRetryDelayMs=500;

    void setState(States s) { state_=s; }
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();
    void handleError();

    
    int removeAndResetChannel();
    void resetChannel();


    EventLoop* loop_;
    InetAddress serverAddr_;//服务器地址
    std::atomic_bool connect_;
    States state_;
    std::unique_ptr<Channel>channel_;

    NewConnectionCallback newConnectionCallback_;
    int retryDelayMs_;

};