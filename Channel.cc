#include"Channel.h"
#include"EventLoop.h"
#include"Logger.h"

#include<sys/epoll.h>
#include<poll.h>
#include <assert.h>
//静态变量类外初始化
//利用poll初始化事件状态
const int Channel::kNoneEvent=0;
const int Channel::kReadEvent=POLLIN | POLLPRI;
const int Channel::kWriteEvent=POLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    ,tied_(false){}


Channel::~Channel()
{
}

//表示该channl正在使用，将tied_置为true
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_=obj;
    tied_=true;
}

//删除没有事件发生的channel
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::update()
{
    loop_->updateChannel(this);
}

// 事件处理函数
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void>guard=tie_.lock();
        //将弱智能指针提升为强智能指针
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
        
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

//执行指定的事件回调函数
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_DEBUG("channel handleEvent revents:%d\n", revents_);

    //获取事件的状态。然后执行相应的回调函数
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
       
    }

    if(revents_&POLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_&(POLLIN |POLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & (POLLOUT))
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}