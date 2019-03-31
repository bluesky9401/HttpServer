
//Channel类，表示每一个客户端连接的通道
// EPOLLIN ：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）；
// EPOLLOUT：表示对应的文件描述符可以写；
// EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；
// EPOLLERR：表示对应的文件描述符发生错误；
// EPOLLHUP：表示对应的文件描述符被挂断；
// EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。
// EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里

#include <iostream>
#include <sys/epoll.h>
#include "Channel.h"

Channel::Channel()
{
    
}

Channel::~Channel()
{
    std::cout << "free Channel" << std::endl;   
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
    else if(events_ & EPOLLOUT)//写事件
    {
        revents_ &= ~EPOLLOUT;
        std::cout << "Event EPOLLOUT" << std::endl;
        writeHandler_();
    }
    else
    {
        std::cout << "Event error" << std::endl;
        revents_ = 0;
        errorHandler_();//连接错误
    }
}
