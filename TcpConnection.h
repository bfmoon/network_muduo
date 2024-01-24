#pragma once
#include"noncopyable.h"
#include"Callbacks.h"
#include"Buffer.h"
#include"InetAddress.h"
#include"Timestamp.h"

#include<memory>
#include<string>
#include<atomic>
class Channel;
class EventLoop;
class Socket;

/**
 * 这意味着你期望TcpConnection的实例由std::shared_ptr管理，
 * 并且希望能够使用shared_from_this()成员函数创建指向相同对象的另一个std::shared_ptr实例。
*/
class TcpConnection:noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    //返回当前的事件循环subloop
    EventLoop* getLoop()const { return loop_;}
    const std::string& name()const {return name_;}
    const InetAddress& localAddress()const { return localAddr_;}
    const InetAddress& peerAddress()const { return peerAddr_;}
    //判断是否连接成功
    bool connected()const { return state_==kConnected;}
    //判断是否未连接
    bool disconnected()const {return state_==kDisconnected;}

    //发送数据，原muduo库是用buffer缓冲区进行数据发送，现在直接改为字符串
    void send(const std::string& buf);

    //断开连接
    void shutdown();

    void forceClose();

    void forceCloseInLoop();

    //设置回调函数
    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_=cb; }
    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_=cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_=cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,size_t highWaterMark)
    { highWaterMarkCallback_=cb; highWaterMark_=highWaterMark;}
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_=cb;}

    //建立连接
    void connectEstablished();
    //销毁连接
    void connectDestroyed();
private:
    //定义连接状态
    enum StateE { kDisconnected,kConnecting,kConnected,kDisconnecting};
    void setState(StateE s){ state_=s;}
    //设置回调操作
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message,size_t len);
    void shutdownInLoop();

private:
    // 这里绝对不是baseLoop， 因为TcpConnection都是在subLoop里面管理的
    EventLoop* loop_;
    const std::string name_;
    std::atomic_int state_; //连接状态
    bool reading_;

    std::unique_ptr<Socket>socket_;
    std::unique_ptr<Channel>channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    //相关回调函数
    ConnectionCallback connectionCallback_;// 有新连接时的回调
    MessageCallback messageCallback_;//有读写消息的回调
    WriteCompleteCallback writeCompleteCallback_;//读写事件完成的回调
    HighWaterMarkCallback highWaterMarkCallback_; 
    CloseCallback closeCallback_;

    size_t highWaterMark_;
    Buffer inputBuffer_;//接收数据缓冲区
    Buffer outputBuffer_;//发送数据缓冲区


};