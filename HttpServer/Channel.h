/* @author: chen tongjie
 * @date:
 * Channel--事件注册器，记录需要监听的套接描述符所感兴趣的事件以及注册有在相应事件
 * 激活时所需进行的操作(回调函数)，主要使用者有TcpServer、EventLoop以及TcpConnection
 * TcpServer应用其监听连接事件，EventLoop应用其用作唤醒任务，TcpConnection应用其监听
 * 可读、可写事件。
 * EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
 * EPOLLOUT：表示对应的文件描述符可以写；
 * EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
 * EPOLLERR：表示对应的文件描述符发生错误；
 * EPOLLHUP：表示对应的文件描述符被挂断；
 * EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
 * EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
 * */
#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <functional>

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
    void notifyFreed()
    {
        freed_ = true;
    }
private:
    /* data */
    int fd_;
    bool freed_;// 用于标记回调函数的资源块是否被释放，防止在使用无效指针
    event_t events_;// Channel希望注册到epoll的事件 
    event_t revents_;// epoll返回的激活事件

    //事件触发时执行的函数
    Callback readHandler_;
    Callback writeHandler_;
    Callback errorHandler_;
    Callback closeHandler_;
};
#endif


