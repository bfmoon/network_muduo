#include"EPollPoller.h"
#include"Logger.h"
#include"Channel.h"

#include<errno.h>
#include<unistd.h>
#include<strings.h>
#include <cassert>

//该变量表示对事件进行的操作
const int kNew=-1;//表示新添加的
const int kAdded=1;//已经添加过的
const int kDeleted=2;//被删除的



EPollPoller::EPollPoller(EventLoop* loop)
    :Poller(loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    ,events_(kInitEventListSize)
{
    if(epollfd_<0)
    {
        LOG_FATAL("EPollPoller::EPollPoller,create epoll error:%d",errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

/**
 * 该类就是执行epoll的操作
 * 首先，epoll_create..
 * 然后，epoll_wait...
 * 接着就是epoll_ctl...操作相关事件
*/
Timestamp EPollPoller::poll(int timeoutMs,ChannelList* activeChannels)
{
    LOG_DEBUG("[ %s :] fd total count:%lu",__FUNCTION__,channels_.size());
    //进行epoll_wait操作
    //epoll_wait的返回值表示发生变化的文件描述符的数量
    int numEvents=::epoll_wait(epollfd_,&*events_.begin(),
                                static_cast<int>(events_.size()),timeoutMs);
    //记录当前错误码
    int savedErrno=errno;
    //获取当前时间
    Timestamp now(Timestamp::now());
    if(numEvents>0)
    {
        LOG_DEBUG(" [%d] ,events happened",numEvents);
        //将有事件发生的文件描述符插入值对应的channelMap中
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents==events_.size())
        {
            //扩容至原来的两倍
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents==0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else    //就是小于0，发生错误
    {
        if (savedErrno!=EINTR)
        {
            //将之前报错的epoll_wait的错误码记录
            errno=savedErrno;
            LOG_ERROR("EPollPoller::poll()");
        }
    }
    return now;
}

// 对事件进行更新操作
void EPollPoller::updateChannel(Channel *channel)
{
    const int index=channel->index();
    LOG_INFO("func=[%s],fd=[%d],events=[%d],index=[%d]",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index==kNew||index==kDeleted)
    {
        //说明是新来的事件，要添加
        
        if(index==kNew)
        {
            int fd=channel->fd();
            channels_[fd]=channel;
        }
        //说明是已经删除的
       
        //设置当前事件的类型标志
        channel->set_index(kAdded);
        //更新操作
        update(EPOLL_CTL_ADD,channel);
    }
    else
    {
        //update existing one with EPOLL_CTL_MOD/DEL
        //就是更改当前事件的状态
        int fd=channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
}

//删除当前channel所属的文件描述符
void EPollPoller::removeChannel(Channel *channel)
{
    int fd=channel->fd();
    //将channel的map表中fd对应的channel移除掉
    channels_.erase(fd);
    LOG_INFO("[%s :]=>fd=[%d]",__FUNCTION__, fd);
    int index=channel->index();
    if(index==kAdded)
    {
        //并将该fd对应的事件从epoll树上移除掉
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

//将发生变化的事件描述符添加到表中记录
void EPollPoller::fillActiveChannels(int numEvents,ChannelList* activeChannels)const
{
    for(int i=0;i<numEvents;i++)
    {
        //取出发生变化的事件的channel
        Channel* channel=static_cast<Channel*>(events_[i].data.ptr);
        
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

//对事件进行处理
void EPollPoller::update(int operation,Channel* channel)
{
    epoll_event event;
    bzero(&event,sizeof event);
    int fd=channel->fd();
    event.events=channel->events();
    event.data.fd=fd;
    event.data.ptr=channel;
    
    
    if(::epoll_ctl(epollfd_,operation,fd,&event)<0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
  
}