#include"TcpServer.h"
#include"EventLoop.h"
#include"Acceptor.h"
#include"EventLoopThreadPool.h"
#include"Logger.h"
#include"TcpConnection.h"
#include"Buffer.h"

#include<string.h>
#include<stdlib.h>

static EventLoop* CheckNotNULL(EventLoop* loop)
{
    if(loop==nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop *loop, 
                    const InetAddress &listerAddr, 
                    const std::string &nameArg,
                    Option option)
        :loop_(CheckNotNULL(loop))
        ,ipPort_(listerAddr.toIpPort())
        ,name_(nameArg)
        ,acceptor_(new Acceptor(loop,listerAddr,option == kReusePort))
        ,threadPool_(new EventLoopThreadPool(loop, name_))
        ,connectionCallback_()
        ,messageCallback_()
        ,nextConnId_(1)
        ,started_(0)
{
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection,this,std::placeholders::_1,std::placeholders::_2)
    );
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr conn(item.second); 
        item.second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if(started_++ == 0)
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(
            std::bind(&Acceptor::listen,acceptor_.get())
        );
    }
}

//建立新连接
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    //根据轮询算法，获取一个loop
    EventLoop* ioLoop=threadPool_->getNextLoop();
    char buf[64]={0};
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName=name_+buf;
    LOG_INFO(" TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    //建立连接的地址信息
    sockaddr_in local;
    ::bzero(&local,sizeof local);
    socklen_t addrlen=sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0)
    {
         LOG_ERROR(" sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(ioLoop,connName,sockfd,localAddr,peerAddr));
    //存放新的连接
    connections_[connName]=conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection,this,std::placeholders::_1)
    );

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished,conn)
    );
}

//这个断开连接的是当前loop不在该线程中执行的
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop,this,conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO(" TcpServer::removeConnectionInLoop [%s] - connection %s\n", 
        name_.c_str(), conn->name().c_str());
    
    //从存放连接的容器中删除
    size_t n=connections_.erase(conn->name());
    EventLoop* ioLoop=conn->getLoop();
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectDestroyed,conn)
    );

}