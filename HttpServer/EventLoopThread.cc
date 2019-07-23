#include <iostream>
#include <sstream>
#include <string>
#include "EventLoopThread.h"
#include "CurrentThread.h"
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
    // 启动线程
    thread_.start();// 创建线程
}
// 用户线程任务
void EventLoopThread::threadFunc()
{
    tid_ = CurrentThread::tid();
    EventLoop loop;
    loop_ = &loop;
    std::cout << "IO thread " << std::to_string(tid_) << " started..." << std::endl;
    loop_->loop();
}
