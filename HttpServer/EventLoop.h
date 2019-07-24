/* author: chentongjie
 * date: 
 * EventLoop--事件分发器(基于Reactor模式) 
 * eventfd: 用于实现多进程或多线程间的事件通知
 * 接口--int eventfd(unsigned int initval, int flags);
 * 用于创建一个eventfd对象并返回其文件描述符
 * eventfd内部维护一个无符号64位(uint64_t)的计数器，initval用于初始化这个计数器的值
 * flags标志可以是EFD_CLOEXEC、EFD_NONBLOCK、EFD_SEMAPHORE的或，可对eventfd描述符进
 * 行read(), write()与close()操作。
 * read()操作时，若内部计数器计数值大于0, 则将该计数值读入缓冲区(注意缓冲区的大小必
 * 须大于或等于8个字节以存放计数值)，并且将计数器的值置为0;若计数器的值等于0并且当前
 * eventfd对象设置为非阻塞，那么read会失败并且将errno设置为EAGAIN，若eventfd对象设置
 * 为阻塞，则阻塞至计数器的值大于0。
 * 进行write操作时，write操作把一个64位整数val写入eventfd，eventfd会尝试将该val加到计
 * 数器上，若求和后的结果溢出，则阻塞等待read()操作或直接返回并将errno设置为EAGAIN
 * */
//IO复用流程的抽象，等待事件，处理事件，执行其他任务

#ifndef _EVENTLOOP_H_
#define _EVENTLOOP_H_
#include <iostream>
#include <functional>
#include <vector>
#include <pthread.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <assert.h>
#include "CurrentThread.h"
#include "MutexLock.h"
#include "Noncopyable.h"
#include "Epoller.h"

class Channel;

class EventLoop : private Noncopyable
{
public:
    typedef std::shared_ptr<Channel> SP_Channel;
    typedef std::function<void()> Functor; //用作回调函数
    typedef std::function<void()> Callback;
    typedef std::vector<SP_Channel> ChannelList;
    EventLoop();
    ~EventLoop();
    // 内部循环处理事件
    void loop();
    // 添加事件到epoller_
    void addChannelToEpoller(SP_Channel spChannel)
    {
        epoller_.addChannel(spChannel);
    }
    // 在连接关闭时注意调用removeChannelToEpoller从epoller_中取消该Channel
    void removeChannelFromEpoller(SP_Channel spChannel)
    {
        epoller_.removeChannel(spChannel);
    }
    // 更新epoller_上的事件
    void updateChannelToEpoller(SP_Channel spChannel)
    {
        epoller_.updateChannel(spChannel);
    }
    void quit()
    {
        quit_ = true;
        if (!isInLoopThread())
            wakeUp();
    }
    
    pid_t getThreadId() const { return tid_; }
     
    void assertInLoopThread()
    {
        if ( !isInLoopThread() )
        {
            std::cout << "Error: EventLoop's function is call in Thread " 
                      << std::to_string(CurrentThread::tid()) << std::endl;
            assert(isInLoopThread());
        } 
    }

    bool isInLoopThread() const { return tid_ == CurrentThread::tid(); }

    // 添加任务事件 Task
    void addTask(Functor &&functor)
    {
        {
            MutexLockGuard lock(mutex_);
            functorList_.push_back(functor);           
        }
        if (!isInLoopThread() || callingPendingFunctors_)
            wakeUp();// 跨线程唤醒
    }
    
    void setOnTimeCallback(int intervals, Callback &&callback);
private:
    void executeTask()
    {
        std::vector<Functor> functorList;
        {
            MutexLockGuard lock(mutex_);
            functorList.swap(functorList_);// 将任务队列置换出来处理
        }      
        callingPendingFunctors_ = true;
        for(Functor &functor : functorList)
        {
            functor();
        }
        callingPendingFunctors_ = false;
    }
    void wakeUp();
    void handleRead();// 用于处理唤醒事件
    void handleError();
    void handleOnTime();
    void startTimer();// 启动定时器
    void stopTimer();// 暂停定时器
    /* data */    
    std::vector<Functor> functorList_;// 作为任务队列
    // ChannelList channelList_;// 保存注册到当前EventLoop的事件注册器 
    ChannelList activeChannelList_;// 激活事件链表
    Epoller epoller_;
    bool quit_;
    bool callingPendingFunctors_;
    const pid_t tid_;
    MutexLock mutex_;
    int wakeUpFd_;// 跨线程唤醒fd (eventfd)
    SP_Channel spWakeUpChannel_;
    int timerFd_;// 定时触发
    SP_Channel spOnTimeChannel_;
    Callback onTimeCallback_;
};
#endif
