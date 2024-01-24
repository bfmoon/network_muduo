#pragma once

#include"noncopyable.h"
#include"Timestamp.h"

#include<memory>
#include<functional>



//类的前置声明，不用包含头文件减少代码的暴露
class EventLoop;

class Channel:noncopyable
{
public:
    //设置事件回调
    using EventCallback= std::function<void()>;
    //读事件回调
    using ReadEventCallback= std::function<void(Timestamp)>;

    Channel(EventLoop* loop,int fd);
    ~Channel();

    //事件处理函数
    void handleEvent(Timestamp receiveTime);

    //设置事件回调函数
    void setReadCallback(ReadEventCallback cb){readCallback_=std::move(cb);}
    void setWriteCallback(EventCallback cb){ writeCallback_=std::move(cb); }
    void setCloseCallback(EventCallback cb){closeCallback_=std::move(cb);}
    void setErrorCallback(EventCallback cb){ errorCallback_=std::move(cb);}


    /*
    将此通道绑定到shared_ ptr管理的所有者对象，防止所有者对象在handleEvent中被销毁。
    */
    void tie(const std::shared_ptr<void>&);

    int fd()const {return fd_;}
    int events()const {return events_;}
    void set_revents(int revt) {revents_= revt;}

    //修改事件的状态，然后更新
    void enableReading(){events_ |=kReadEvent;update();}
    void disenableReading(){events_&=~kReadEvent;update();}
    void enableWriteing(){events_|=kWriteEvent;update();}
    void disenableWriteing(){events_&=~kWriteEvent;update();}
    void disableAll(){events_=kNoneEvent;update();}

    // 返回fd当前的事件状态
    bool isNoneEvent(){return events_==kNoneEvent;}
    bool isReading(){return events_==kReadEvent;}
    bool isWriteing(){return events_==kWriteEvent;}

    // for Polle
    int index(){return index_;}
    void set_index(int idx){index_=idx;}

    //返回当前线程的事件循环
    EventLoop* ownerLoop(){return loop_;}
    void remove();

private:
    //将设置的事件状态更新
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

private:

    //下面三个变量表示事件类型
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;
    const int fd_;
    int events_;//表示fd感兴趣的事件
    int revents_; //Poller具体返回发生的事件
    int index_; //Poller中使用

    //由于每个线程有独自的一个channel，利用强弱智能指针
    //以及bool变量表示该channel是否在使用
    std::weak_ptr<void>tie_;
    bool tied_;

    //定义事件回调对象
    //因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};