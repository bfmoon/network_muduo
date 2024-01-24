//该函数是用于区分使用哪种IO复用技术
//而muduo库则是默认使用的epoll

#include"Poller.h"
#include"EPollPoller.h"

#include<stdlib.h>


Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    //就是如果存在环境变量MUDUO_USE_POLL，则表示使用pollIO复用技术
    if(::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;
    }
    else
    {
        return new EPollPoller(loop);
    }
}