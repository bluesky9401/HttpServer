// Copyright 2019, Chen Shuaihao.
//
//Author: Chen Shuaihao
//
//ThreadPool类，简易线程池实现，表示worker线程,执行通用任务线程
//
// 使用的同步原语有 
// pthread_mutex_t mutex_l;//互斥锁
// pthread_cond_t condtion_l;//条件变量
// 使用的系统调用有
// pthread_mutex_init();
// pthread_cond_init();
// pthread_create(&thread_[i],NULL,threadFunc,this)
// pthread_mutex_lock()
// pthread_mutex_unlock()
// pthread_cond_signal()
// pthread_cond_wait()
// pthread_cond_broadcast();
// pthread_join()
// pthread_mutex_destory()
// pthread_cond_destory()

#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <mutex>  
#include <thread> 
#include <condition_variable>
#include <functional>

class ThreadPool
{
public:
    typedef std::function<void()> Task;
    ThreadPool(int threadnum = 0);
    ~ThreadPool();

    void start();
    void Stop();
    int addTask(Task task);
    void ThreadFunc();
    int getThreadNum()
    { return threadnum_; }

private:
    bool started_;
    int threadnum_;
    std::vector<std::thread*> threadlist_;
    std::queue<Task> taskqueue_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

#endif
