/* @author: chentongjie
 * @date:
 * @description: EventLoopThreadPool--IO线程池
 * */
#ifndef _EVENTLOOP_THREAD_POOL_H_
#define _EVENTLOOP_THREAD_POOL_H_

#include <vector>
#include "EventLoop.h"
#include "EventLoopThread.h"

class EventLoopThreadPool
{
public:
    EventLoopThreadPool(EventLoop *mainLoop, int threadNum = 0);
    ~EventLoopThreadPool();

    void start();
    EventLoop* getNextLoop();

private:
    EventLoop *mainLoop_;
    int threadNum_;
    int index_;
    std::vector<EventLoopThread> threadList_;
};

#endif
