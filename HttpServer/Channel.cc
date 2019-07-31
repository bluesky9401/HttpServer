
#include <iostream>
#include <sys/epoll.h>
#include "Channel.h"
using std::cout;
using std::endl;
Channel::Channel() : freed_(false)
{ }

Channel::~Channel()
{
//    std::cout << "free Channel" << std::endl;   
}

void Channel::handleEvent()
{
    if (freed_) return;
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))//对方异常关闭事件
    {
        std::cout << "Event EPOLLRDHUP" << std::endl;
        if (closeHandler_)
            closeHandler_();
    }

    if (freed_) return;
    if(revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))//读事件，对端有数据或者正常关闭
    {
       // std::cout << "Event EPOLLIN" << std::endl;
        readHandler_();
    }

    if (freed_) return;
    if(revents_ & EPOLLOUT)//写事件
    {
        writeHandler_();
    }

    if (freed_) return;
    if (revents_ & EPOLLERR)
    {
        // std::cout << "Event error" << std::endl;
        if (errorHandler_)
            errorHandler_();//连接错误
    }
}
