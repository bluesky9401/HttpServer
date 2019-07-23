//Epoller类，对epoll的封装

#ifndef _EPOLLER_H_
#define _EPOLLER_H_

#include <vector>
#include <pthread.h>
#include <map>
#include <memory>
#include <sys/epoll.h>
#include "Channel.h"
//class Channel;

class Epoller
{
public:
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::vector<SP_Channel> ChannelList;
    Epoller();
    ~Epoller();

    void epoll(ChannelList &activeChannelList);
    void addChannel(SP_Channel spChannel);
    void removeChannel(SP_Channel spChannel);
    void updateChannel(SP_Channel spChannel);
private:
    void fillActiveChannelList();
    int epollfd_;
    std::vector<struct epoll_event> eventList_;// 用于在epoll_wait保存返回的激活事件
    std::map<int, SP_Channel> channelMap_;// 记录fd--Channel*的map映射表
                                        // 记录当前登记到epoller的Channel
    
};
#endif


