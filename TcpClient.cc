#include "TcpClient.h"
#include "Connector.h"
#include"Logger.h"
#include"EventLoop.h"

#include<mutex>
#include<memory>
#include<unistd.h>
#include<string>
static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

//客户端断开连接释放资源
static void removeConnectionCallback(EventLoop* loop,const TcpConnectionPtr& conn)
{
    loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
}

TcpClient::TcpClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &nameArg)
                :loop_(CheckLoopNotNull(loop))
                ,connector_(new Connector(loop,serverAddr))
                ,name_(nameArg)
                ,connectionCallback_()
                ,messageCallback_()
                ,connect_(true)
                ,nextConnId_(1)
{
    connector_->setNewConnectionCallback(std::bind(&TcpClient::newConnection,
                        this,std::placeholders::_1));
    LOG_INFO(" TcpClient::TcpClient [ %s ] - connector [%p]\n",name_.c_str(),connector_.get());
}
TcpClient::~TcpClient()
{
    LOG_INFO("TcpClient::~TcpClient [ %s ] - connector %p",name_.c_str(),connector_.get());
    TcpConnectionPtr conn;
    bool unique=false;
    {
        std::lock_guard<std::mutex>lock(mutex_);
        unique=connection_.unique();
        conn=connection_;
    }
    if(conn)
    {
        CloseCallback cb=std::bind(removeConnectionCallback,loop_,std::placeholders::_1);
        loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback,conn,cb));
        if(unique)
        {
            conn->forceClose();
        }
    }
    else
    {
        connector_->stop();
        loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed,connection_));
    }
}

void TcpClient::connect()
{
    LOG_INFO("TcpClient::connect[%s]-connecting to [%s]\n",name_.c_str(),connector_->serverAddr().toIpPort().c_str());
    connect_=true;
    connector_->start();
}
void TcpClient::disconnect()
{
    connect_=false;
    {
        std::lock_guard<std::mutex>lock(mutex_);
        if(connection_)
        {
            connection_->shutdown();
        }
    }
}
void TcpClient::stop()
{
    connect_=false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
        //建立连接的地址信息
    sockaddr_in peer;
    ::bzero(&peer,sizeof peer);
    socklen_t addrlen=sizeof peer;
    if(::getsockname(sockfd,(sockaddr*)&peer,&addrlen)<0)
    {
         LOG_ERROR(" sockets::getpeerAddr");
    }
    InetAddress peerAddr(peer);
    char buf[32]={0};
    snprintf(buf,sizeof buf,":%s#%d",peerAddr.toIpPort().c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName=name_+ buf;

    //建立连接的地址信息
    sockaddr_in local;
    ::bzero(&local,sizeof local);
    addrlen=sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0)
    {
         LOG_ERROR(" sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(loop_,connName,sockfd,localAddr,peerAddr));

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(std::bind(&TcpClient::removeConnection,this,
                                        std::placeholders::_1));
    
    {
        std::unique_lock<std::mutex>lock(mutex_);
        connection_=conn;
    }
    conn->connectEstablished();

}

void TcpClient::removeConnection(const TcpConnectionPtr &conn)
{
    {
        std::unique_lock<std::mutex>lock(mutex_);
        connection_.reset();
    }
    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
    if(connect_)
    {
        LOG_INFO("TcpClient::connect [%s] - Reconnecting to ",connector_->serverAddr().toIpPort());
        connector_->restart();
    }
}