/* @author: chen tongjie
 * @date: 
 * 
 * */
#include <iostream>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include "EventLoop.h"
#include "CurrentThread.h"
using std::cout;
using std::endl;
/* 创建一个eventfd对象并返回其描述符 */
int createEventFd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        cout << "Failed in eventfd" << endl;
        exit(1);
    }
    return evtfd;
}

int createTimerFd(int intervals) 
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
    if (timerfd <= 0)
    {
        cout << "Failed in create timerfd" << endl;
    }
    struct itimerspec its;
    its.it_value.tv_sec = 2;    // 第一次在2s后触发可读
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1; // interval
    its.it_interval.tv_nsec = 0;
    if (::timerfd_settime(timerfd, 0, &its, nullptr) != 0) 
    {
        cout << "Fail to set timerfd!" << endl;
    }
    return timerfd;
}
EventLoop::EventLoop(/* args */)
    : functorList_(),
      activeChannelList_(),
      epoller_(),
      quit_(true),
      callingPendingFunctors_(false),
      tid_(CurrentThread::tid()),
      mutex_(),
      wakeUpFd_(createEventFd()),
      spWakeUpChannel_(new Channel()),
      spOnTimeChannel_(new Channel())
{
    cout << "EventLoop's Thread ID" << std::to_string(tid_) << endl; 
    spWakeUpChannel_->setFd(wakeUpFd_);// 在EventLoop创建的同时将唤醒事件添加到epoller_
    spWakeUpChannel_->setEvents(EPOLLIN | EPOLLET);
    spWakeUpChannel_->setReadHandle(std::bind(&EventLoop::handleRead, this));
    spWakeUpChannel_->setErrorHandle(std::bind(&EventLoop::handleError, this));
    addChannelToEpoller(spWakeUpChannel_);
}

EventLoop::~EventLoop()
{
    close(wakeUpFd_);
}

void EventLoop::wakeUp()
{
    uint64_t one = 1;
    // 注意每次写入8个字节的数据
    ssize_t n = write(wakeUpFd_, (char*)(&one), sizeof one);
}

/* 在eventfd可读时(内部计数值大于0)，epoller_ epoll_wait事件返回
 * 然后执行handleRead函数，主要用于唤醒线程处理后面任务队列中的事件
 * */
void EventLoop::handleRead()
{
    uint64_t one = 1;
    // read操作会重新更新内部计数值为0
    ssize_t n = read(wakeUpFd_, &one, sizeof one);
}

void EventLoop::handleError()
{
    ;
}
void EventLoop::handleOnTime()
{
    uint64_t tmp;
    read(timerFd_, &tmp, sizeof(uint64_t));
    onTimeCallback_();
}
// 设置EventLoop上的定时事件
void EventLoop::setOnTimeCallback(int intervals, Callback &&callback) 
{
    onTimeCallback_ = callback;
    timerFd_ = createTimerFd(intervals);
    spOnTimeChannel_->setFd(timerFd_);
    spOnTimeChannel_->setEvents(EPOLLIN | EPOLLET);
    spOnTimeChannel_->setReadHandle(std::bind(&EventLoop::handleOnTime, this));
    addChannelToEpoller(spOnTimeChannel_); 
}
// 启动定时器
void startTimer();
// 暂停定时器
void stopTimer();

void EventLoop::loop()
{
    quit_ = false;
    while(!quit_)
    {
        epoller_.epoll(activeChannelList_);// activeChannelList为值-结果参数，
                                           // 用于存放激活的Channel链表
        for(SP_Channel spChannel : activeChannelList_)// 处理激活的事件
        {            
            spChannel->handleEvent();//处理事件
        }
        activeChannelList_.clear();// 清空
        executeTask();// 执行任务列表中的事件
    }
}
