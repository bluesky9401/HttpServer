#include <string>
using std::string;
#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *mainLoop, int threadNum)
    : mainLoop_(mainLoop),
      threadNum_(threadNum),
      threadList_(threadNum),
      index_(0)
{
    for(int i = 1; i <= threadNum_; ++i)
    {
        threadList_[i-1].setName(string("IO thread") + std::to_string(i));
    }
}

EventLoopThreadPool::~EventLoopThreadPool()
{
   // for(int i = 0; i < threadNum_; ++i)
   // {
   //     delete threadList_[i];
   // }
   // threadList_.clear();
}

void EventLoopThreadPool::start()
{
    if(threadNum_ > 0)
    {
        for(int i = 0; i < threadNum_; ++i)
        {
            threadList_[i].start();
        }
    }
    else
    {
        std::cout << "the thread's number of EventLoopThreadPool is less than 1";
    }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    if(threadNum_ > 0)
    {
        //RR策略
        EventLoop *loop = threadList_[index_].getLoop();
        index_ = (index_ + 1) % threadNum_;
        return loop;
    }
    else
    {
        return mainLoop_;
    }
}
