/* @author: chen tongjie
 * @date:
 * @description: posix线程封装
 * */
#ifndef _THREAD_H_
#define _THREAD_H_

#include <functional>
#include <memory>
#include <pthread.h>
#include <string>

class Thread
{
public:
    typedef std::function<void ()> ThreadFunc;// 线程实际要做的工作

    explicit Thread(const ThreadFunc&, const std::string& name = std::string());
    ~Thread();

    void start();// 开始(创建一个线程并且运行线程函数)
    int join();// 用于主线程等待子线程的结束
    bool started() const { return started_; }// 返回线程的运行状态
    pthread_t getThreadId() const { return pthreadId_; }
    const std::string& name() const { return name_; }

private:
    void setDefaultName();

    bool started_;// 表示线程的运行状态
    bool joined_;// 用于判断线程是否已销毁
    pthread_t pthreadId_;// 线程在进程中的id
    ThreadFunc func_;// 线程实际工作
    std::string name_;// 线程名字
};

// 为了在线程中保留name,tid这些数据
// 用作pthread_create()函数中线程函数形参指针所指向的结构
struct ThreadData
{
    typedef Thread::ThreadFunc ThreadFunc;
    ThreadFunc func_;

    ThreadData(const ThreadFunc &func) : func_(func)
    { }
};
#endif
