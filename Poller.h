#pragma once
#include"noncopyable.h"
#include"EventLoop.h"
#include"Timestamp.h"

#include<vector>
#include<unordered_map>

//Channel类的前置声明
class Channel;
class EventLoop;

class Poller:noncopyable
{
public:
    using ChannelList=std::vector<Channel*>;
    Poller(EventLoop* loop);
    virtual ~Poller()=default;
    
    //该函数涉及多种IO复用技术，poll和epoll，并且可以添加selectIO复用技术
    //由于是纯虚函数，要在派生类中重写该函数
    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannel)=0;
    //对事件进行更新操作
    virtual void updateChannel(Channel* channel)=0;
    virtual void removeChannel(Channel* channel)=0;

    //判断channel是否在poller中
    bool hasChannel(Channel* channel) const;
    
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap=std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    //当前channel所属的事件循环
    EventLoop* ownerLoop_;
};