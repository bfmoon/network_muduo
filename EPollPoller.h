#pragma once
#include"Poller.h"

#include<sys/epoll.h>
#include<vector>

class EPollPoller:public Poller
{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller()override;

    Timestamp poll(int timeoutMs,ChannelList* activeChannels)override;
    //对事件进行更新操作
    void updateChannel(Channel* channel)override;

    void removeChannel(Channel* channel)override;

private:
    //默认事件列表大小为16
    static const int kInitEventListSize=16;

    //填入活跃的channel事件
    void fillActiveChannels(int numEvents,ChannelList* activeChannels)const;

    //对epoll事件进行epoll_ctl操作
    void update(int operation,Channel* channel);

    using EventList=std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};