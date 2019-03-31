#include "EventLoopThreadPool.h"
#include <string>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *mainLoop, int threadNum)
    : mainLoop_(mainLoop),
    threadNum_(threadNum),
    threadList_(),
    index_(0)
{
    for(int i = 0; i < threadNum_; ++i)
    {
        EventLoopThread *pEventLoopThread = new EventLoopThread(std::to_string(i));
        threadList_.push_back(pEventLoopThread);
    }
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    for(int i = 0; i < threadNum_; ++i)
    {
        delete threadList_[i];
    }
    threadList_.clear();
}

void EventLoopThreadPool::start()
{
    if(threadNum_ > 0)
    {
        for(int i = 0; i < threadNum_; ++i)
        {
            threadList_[i]->start();
        }
    }
    else
    {
        std::cout << "the thread's number of EventLoopThreadPool is 0";
    }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    if(threadNum_ > 0)
    {
        //RR策略
        EventLoop *loop = threadList_[index_]->getLoop();
        index_ = (index_ + 1) % threadNum_;
        return loop;
    }
    else
    {
        return mainLoop_;
    }
}
