#include <memory>
#include <iostream>
#include <string>
#include <cassert>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include "Thread.h"
#include "CurrentThread.h"
using std::string;

namespace CurrentThread
{
    __thread int t_cachedTid = 0;
    __thread char t_tidString[32];
    __thread int t_tidStringLength = 6;
    __thread const char* t_threadName = "default";
}

pid_t gettid()
{
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

void CurrentThread::cacheTid()
{
    if (t_cachedTid == 0)
    {
        t_cachedTid = gettid();
        t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
    }
}
// 为了在线程中保留name,tid这些数据
// 用作pthread_create()函数中线程函数形参指针所指向的结构
struct ThreadData
{
    typedef Thread::ThreadFunc ThreadFunc;

    ThreadData(const ThreadFunc &func, const string &name, pid_t *tid) 
    : func_(func),
      name_(name),
      tid_(tid)
    { }
    void runInThread()
    {
        *tid_ = CurrentThread::tid();// 给当前线程回传线程ID 
        tid_ = nullptr;
        CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();
        prctl(PR_SET_NAME, CurrentThread::t_threadName);// 给当前线程命名
        func_();// 调用用户线程任务
        CurrentThread::t_threadName = "finished";
    }

    ThreadFunc func_;
    string name_;
    pid_t *tid_;
};

void *startThread(void *obj)// 封装线程实际运行的函数
{
    std::shared_ptr<ThreadData> spData(static_cast<ThreadData*>(obj));
    spData->runInThread();
    return nullptr;
}

Thread::Thread(const ThreadFunc &func, const std::string n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
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

void Thread::start()
{
    assert(!started_);
    started_ = true;
    ThreadData* data = new ThreadData(func_, name_, &tid_);
    if (pthread_create(&pthreadId_, NULL, &startThread, data))
    {
        // 线程创建失败
        started_ = false;
        delete data;
    }
}

int Thread::join()
{
    // 主要由主线程调用，用于等待子线程结束然后回收资源
    assert(started_);
    assert(!joined_);
    joined_ = true;
    return pthread_join(pthreadId_, NULL);
}
