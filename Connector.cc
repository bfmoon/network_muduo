#include "Connector.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"

#include <error.h>
#include <unistd.h>
#include <stdio.h>  // snprintf
#include <sys/socket.h>
#include <sys/uio.h>
const int Connector::kMaxRetryDelayMs;
const int Connector::kInitRetryDelayMs;

static int createNonblockingOrDie()
{
    int sockfd=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d: create listen sockfd error:%d\n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    return sockfd;
    
}

Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
                :loop_(loop)
                ,serverAddr_(serverAddr)
                ,state_(kDisconnected)
                ,retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG(" [func: %s],ctor[%s]",__FUNCTION__,this);
}
Connector::~Connector()
{
    LOG_DEBUG(" [func: %s],dtor[%s]",__FUNCTION__,this);
}

void Connector::start()
{
    connect_=true;
    loop_->runInLoop(std::bind(&Connector::startInLoop,this));
}


void Connector::restart()
{
    setState(kDisconnected);
    retryDelayMs_=kInitRetryDelayMs;
    connect_=true;
    startInLoop();
}

void Connector::stop()
{
    connect_=false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop,this));
}

void Connector::startInLoop()
{
    if(connect_)
    {
        connect();
    }
    else
    {
        LOG_DEBUG(" [func : %s ],do not connect",__FUNCTION__);
    }
}
void Connector::stopInLoop()
{
    if(state_==kConnecting)
    {
        setState(kDisconnected);
        int sockfd=removeAndResetChannel();
    }
}

void Connector::connect()
{
    int sockfd=createNonblockingOrDie();
    int ret=::connect(sockfd,(sockaddr*)serverAddr_.getSockAddr(),static_cast<socklen_t>(sizeof (sockaddr_in)));

    int saveErrno=(ret==0)?0:errno;
    switch(saveErrno)
    {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            LOG_ERROR(" connec error in Connector::startInLoop : [%d]",saveErrno);
            ::close(sockfd);
            break;
        
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_ERROR(" connec error in Connector::startInLoop : [%d]",saveErrno);
            break;

        default:
            LOG_ERROR(" Unexpected error in COnnector::startInLoop: [%d]",saveErrno);
            ::close(sockfd);
            break;
    }
}
void Connector::connecting(int sockfd)
{
    //第一次，少了这几句话，导致一直无法连接
    setState(kConnecting);
    channel_.reset(new Channel(loop_,sockfd));
    channel_->setWriteCallback(std::bind(&Connector::handleWrite,this));

    channel_->setErrorCallback(std::bind(&Connector::handleError,this));

    channel_->enableWriteing();
}

//handleWrite方法是用来处理连接事件的回调函数
void Connector::handleWrite()
{
    LOG_DEBUG(" Connector::handleWrite [%d]",state_);
    if(state_==kConnecting)
    {
        int sockfd=removeAndResetChannel();
        
        int optval;
        socklen_t optlen=static_cast<socklen_t>(sizeof optval);
        int err=(::getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)?errno:optval;
        if(err)
        {
            LOG_ERROR(" Connector::handleWrite - SO_ERROR = [%d]",err);
        }
        else
        {
            setState(kConnected);
            if(connect_)
            {
                newConnectionCallback_(sockfd);
            }
            else
            {
                ::close(sockfd);
            }
        }   
    }

}


void Connector::handleError()
{
    LOG_ERROR(" Connector::handleError state=[%s]",state_);
    if(state_==kConnecting)
    {
        int sockfd=removeAndResetChannel();
        int optval;
        socklen_t optlen=static_cast<socklen_t>(sizeof optval);
        int err=((::getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&optval,&optlen))<0?errno:optval);
    }
}

int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();

    int sockfd=channel_->fd();

    loop_->queueInLoop(std::bind(&Connector::resetChannel,this));

    return sockfd;
}
void Connector::resetChannel()
{
    channel_.reset();
}