#include <iostream>
#include <sstream>
#include "EventLoopThread.h"

EventLoopThread::EventLoopThread(std::string n)
    : thread_(std::bind(&EventLoopThread::threadFunc, this)),
      loop_(NULL),
      name_(n)
{
    
}

EventLoopThread::~EventLoopThread()
{
    // 线程结束时清理
    // 一般是在主线程释放EventLoopThreadPool时所进行的操作
    loop_->quit();// 设置quit_位，唤醒线程执行剩余任务，然后结束loop循环
    thread_.join();// 等待IO线程结束
    std::cout << "IO thread " << name_ << " finished" << std::endl;
}

EventLoop* EventLoopThread::getLoop()
{
    return loop_;
}
void EventLoopThread::start()
{
    //create thread
    thread_.start();// 创建线程
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    loop_ = &loop;

    threadId_ = thread_.getThreadId();
    std::cout << "IO thread " << name_ << " started" << std::endl;
    loop_->loop();
}
