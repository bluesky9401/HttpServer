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

    explicit Thread(const ThreadFunc&, const std::string name = "Thread");
    ~Thread();

    void start();// 开始(创建一个线程并且运行线程函数)
    int join();// 用于主线程等待子线程的结束
    bool started() const { return started_; }// 返回线程的运行状态
    pid_t getThreadId() const { return tid_; }
    const std::string& name() const { return name_; }

private:
    void setDefaultName();

    bool started_;// 表示线程的运行状态
    bool joined_;// 用于判断线程是否已销毁
    pthread_t pthreadId_;// 线程在进程中的id
    pid_t tid_;
    ThreadFunc func_;// 线程实际工作
    std::string name_;// 线程名字
};

#endif
