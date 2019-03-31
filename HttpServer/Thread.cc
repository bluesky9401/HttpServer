#include "Thread.h"
#include <memory>
#include <iostream>
#include <string>
#include <assert.h>
void *startThread(void *obj)// 封装线程实际运行的函数
{
    std::shared_ptr<ThreadData> spData(static_cast<ThreadData*>(obj));
    spData->func_();
    return NULL;
}

Thread::Thread(const ThreadFunc &func, const std::string &n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    func_(func),
    name_(n)
{
}

Thread::~Thread()
{
    // 若Thread对象析构时，线程还在运行则应用
    // pthread_detach将线程转变为unjoinable状
    // 态，这样在线程结束后就自动释放其所占有
    // 的资源
    if (started_ && !joined_)
        pthread_detach(pthreadId_);
}

void Thread::setDefaultName()
{
    if (name_.empty())
    {
        name_ = std::string("thread") + std::to_string(pthreadId_);
    }
}

void Thread::start()
{
    assert(!started_);
    started_ = true;
    ThreadData* data = new ThreadData(func_);
    if (pthread_create(&pthreadId_, NULL, &startThread, data))
    {
        started_ = false;
        delete data;
    }
    setDefaultName();
}

int Thread::join()
{
    // 主要由主线程调用，用于等待子线程结束然后回收资源
    assert(started_);
    assert(!joined_);
    joined_ = true;
    return pthread_join(pthreadId_, NULL);
}
