
#include <iostream>
#include <sys/epoll.h>
#include "Channel.h"
using std::cout;
using std::endl;
Channel::Channel()
{ }

Channel::~Channel()
{
//    std::cout << "free Channel" << std::endl;   
}

void Channel::handleEvent()
{
    if(revents_ & EPOLLRDHUP)//对方异常关闭事件
    {
        std::cout << "Event EPOLLRDHUP" << std::endl;
        revents_ &= ~EPOLLRDHUP;
        closeHandler_();
    }
    else if(revents_ & (EPOLLIN | EPOLLPRI))//读事件，对端有数据或者正常关闭
    {
        //std::cout << "Event EPOLLIN" << std::endl;
        revents_ &= ~(EPOLLIN | EPOLLPRI);
        readHandler_();
    }
    else if(revents_ & EPOLLOUT)//写事件
    {
        revents_ &= ~EPOLLOUT;
        writeHandler_();
    }
    else
    {
        std::cout << "Event error" << std::endl;
        revents_ = 0;
        errorHandler_();//连接错误
    }
}
