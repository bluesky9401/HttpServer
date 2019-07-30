//    typedef union epoll_data {
//        void    *ptr;
//        int      fd;
//        uint32_t u32;
//        uint64_t u64;
//    } epoll_data_t;

//    struct epoll_event {
//        uint32_t     events;    /* Epoll events */
//        epoll_data_t data;      /* User data variable */
//    };

#include <iostream>
#include <cstdio> //perror
#include <cstdlib> //exit
#include <unistd.h> //close
#include <errno.h>
#include <cstring>
#include "Epoller.h"
using std::cout;
using std::cin;
using std::endl;
#define EVENTNUM 4096 //最大触发事件数量
epoll_event init;

Epoller::Epoller()
    : epollfd_(-1),
      eventList_(EVENTNUM),
      channelMap_()
{
    epollfd_ = epoll_create(256);
    if(epollfd_ == -1)
    {
        perror("epoll_create");
        exit(1);
    }
    std::cout << "epoll_create" << epollfd_ << std::endl;
}

Epoller::~Epoller()
{
    close(epollfd_);
}

// 等待I/O事件
void Epoller::epoll(ChannelList &activeChannelList)
{
    int nfds = epoll_wait(epollfd_, &*eventList_.begin(), static_cast<int>(eventList_.size()), -1);
    
    if(nfds == -1)
    {
        printf("error code is:%d", errno);
        perror("epoll wait error");
        //exit(1)
    }
    for(int i = 0; i < nfds; ++i)
    {
        int events = eventList_[i].events;
        int fd = eventList_[i].data.fd;
        auto iter = channelMap_.find(fd);
        if(iter != channelMap_.end())
        {
            SP_Channel spChannel = channelMap_[fd];
            spChannel->setREvents(events);
            activeChannelList.push_back(spChannel);
        }
        else
        {            
            std::cout << "not find channel!" << std::endl;
        }
    }
    // 若当前事件链表不足以存放epoll返回的事件数量，则扩容
    if(nfds == static_cast<int> (eventList_.size()) )
    {
        std::cout << "resize:" << nfds << std::endl;
        eventList_.resize(nfds * 2);
    }
}

//添加事件
void Epoller::addChannel(SP_Channel spChannel)
{
    int fd = spChannel->getFd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = spChannel->getEvents();
    //data是联合体
    ev.data.fd = fd;
    channelMap_[fd] = spChannel;

    if(epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll add error");
        exit(1);
    }
}

//删除事件
void Epoller::removeChannel(SP_Channel spChannel)
{
    int fd = spChannel->getFd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = spChannel->getEvents();
    ev.data.fd = fd;
    channelMap_.erase(fd);// 从映射表中删除对应元素    

    if(epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev) == -1)
    {
        perror("epoll del error");
        exit(1);
    }
}

//更新事件
void Epoller::updateChannel(SP_Channel spChannel)
{
    int fd = spChannel->getFd();
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = spChannel->getEvents();
    ev.data.fd = fd;

    if(epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev) == -1)
    {
        perror("epoll update error");
        exit(1);
    }
}

