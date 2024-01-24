#include"TcpConnection.h"
#include"Logger.h"
#include"Channel.h"
#include"EventLoop.h"
#include"Socket.h"

#include<error.h>
#include<functional>
#include <sys/types.h>         
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include<string>
static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
              const std::string &name,
              int sockfd,
              const InetAddress &localAddr,
              const InetAddress &peerAddr)
    :loop_(CheckLoopNotNull(loop))
    ,name_(name)
    ,state_(kConnecting)
    ,reading_(true)
    ,socket_(new Socket(sockfd))
    ,channel_(new Channel(loop,sockfd))
    ,localAddr_(localAddr)
    ,peerAddr_(peerAddr)
    ,highWaterMark_(64*1024*1024) // 64M
{
    // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    //读事件回调
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1)
        );
    //写事件回调
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite,this)
    );
    //关闭事件回调
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose,this)
    );
    //错误事件回调
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError,this)
    );

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

//发送数据
void TcpConnection::send(const std::string &buf)
{
    //如果当前已经连接了
    if(state_==kConnected)
    {
        //如果当前的loop在该线程中
        if(loop_->isInLoopThread())
        {
            //调用发送函数
            sendInLoop(buf.c_str(),buf.size());
        }
        else
        {
            //就调用runloop函数直接在当前的loop中执行发送函数
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop,this,buf.c_str(),buf.size())
            );
        }
    }
}

// 断开连接
void TcpConnection::shutdown()
{
    if(state_==kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));;
    }
}

void TcpConnection::forceClose()
{
    if(state_==kConnected || state_==kDisconnecting)
    {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop,shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop()
{
    if(state_==kConnected|| state_==kDisconnecting)
    {
        handleClose();
    }
}

// 建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}
// 销毁连接
void TcpConnection::connectDestroyed()
{
    if(state_==kConnected)
    {
        setState(kDisconnected);
        //忽略所有事件
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

//读事件回调
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno=0;
    ssize_t n=inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0)
    {
        //说明读取成功
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n==0)
    {
        //说明没有数据，关闭
        handleClose();
    }
    else
    {
        errno=savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if(channel_->isWriteing())
    {
        int savedErrno = 0;
        //发送数据
        ssize_t n=outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n>0)
        {
            outputBuffer_.retrieve(n);
            //如果outputBuffer的接收(可读缓冲区满了)
            if(outputBuffer_.readableBytes()==0)
            {
                //则断开发送事件的状态
                channel_->disenableWriteing();
                if(writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(
                        std::bind(&TcpConnection::writeCompleteCallback_,shared_from_this())
                    );
                }
                if(state_==kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);// 执行连接关闭的回调
    closeCallback_(connPtr);// 关闭连接的回调  执行的是TcpServer::removeConnection回调方法
}
void TcpConnection::handleError()
{
    int optval=0;
    socklen_t optlen=static_cast<socklen_t>(sizeof optval);
    int err = 0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        err= errno;
    }
    else
    {
        err=optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

void TcpConnection::sendInLoop(const void *message, size_t len)
{
    ssize_t nwrote=0;
    size_t remaining=len;
    bool faultError=false;
    //如果已经断开连接
    if(state_==kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    // 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriteing()&& outputBuffer_.readableBytes()==0)
    {
        //发送数据
        nwrote=::write(channel_->fd(),message,len);
        if(nwrote>=0)
        {
            remaining=len-nwrote;
            if(remaining==0&& writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(&TcpConnection::writeCompleteCallback_,shared_from_this())
                );
            }
        }
        else
        {
            nwrote=0;
            if(errno!=EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno==EPIPE || errno==ECONNRESET)
                {
                    faultError=true;
                }
            }
        }
    }

     // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
    // 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
    // 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    if(!faultError && remaining>0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldlen=outputBuffer_.readableBytes();
        if(oldlen+remaining >=highWaterMark_ && oldlen< highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_,shared_from_this(),oldlen+remaining)
            );
        }
        outputBuffer_.append((char*)message+nwrote,remaining);
        if(!channel_->isWriteing())
        {
            // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
            channel_->enableWriteing();
        }
    }
}
void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriteing())// 说明outputBuffer中的数据已经全部发送完成
    {
        socket_->shutdownWrite();// 关闭写端
    }
}