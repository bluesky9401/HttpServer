/* @author: chen tongjie
 * @date:
 * Channel--事件注册器，记录需要监听的套接描述符所感兴趣的事件以及注册有在相应事件
 * 激活时所需进行的操作(回调函数)，主要使用者有TcpServer、EventLoop以及TcpConnection
 * TcpServer应用其监听连接事件，EventLoop应用其用作唤醒任务，TcpConnection应用其监听
 * 可读、可写事件。
 * */
#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <functional>

// const int kNew = -1;
// const int kAdded = 1;
// const int kDeleted = 2;

class Channel
{
public:
    typedef std::function<void()> Callback;
    typedef uint32_t event_t;

    Channel();
    ~Channel();

    void setFd(int fd) 
    {
        fd_ = fd; 
    }
    int getFd() const
    { 
        return fd_; 
    }
    /* 由用户使用，控制该Channel注册器所绑定描述符感兴趣的事件 */
    void setEvents(event_t events)
    { 
        events_ = events; 
    }
    /* 由epoller使用，用于返回激活的事件 */
    void setREvents(event_t events) 
    {
        revents_ = events;
    }

    event_t getEvents() const
    { 
        return events_; 
    }
//    int getIndex() const
//    {
//        return index_;
//    }

    void handleEvent();

    // 设置Channel注册器上的回调函数
    void setReadHandle(Callback &&cb)
    {
        readHandler_ = cb; 
    }
    void setWriteHandle(Callback &&cb)
    {
        writeHandler_ = cb; 
    }    
    void setErrorHandle(Callback &&cb)
    { 
        errorHandler_ = cb;
    }
    void setCloseHandle(Callback &&cb)
    {
        closeHandler_ = cb;
    }

private:
    /* data */
    int fd_;
  //  int index_;// 用于标记当前的Channel是否添加到epoller_

    event_t events_;// Channel希望注册到epoll的事件 
    event_t revents_;// epoll返回的激活事件

    //事件触发时执行的函数，在tcpconn中注册
    Callback readHandler_;
    Callback writeHandler_;
    Callback errorHandler_;
    Callback closeHandler_;
};
#endif


