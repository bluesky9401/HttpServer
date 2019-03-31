
/* Author: Chen TongJie *
 * date: 2019.3.15 */
//ThreadPool类，简易线程池实现，表示worker线程,执行通用任务线程
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

#include <vector>
#include <pthread.h>
#include <functional>

void* threadFunc(void* tp);// 线程函数
class ThreadPool
{
public:
    friend void* threadFunc(void* args);
    typedef std::function<void()> Task_t;
    ThreadPool(unsigned threadNum = 0, unsigned maxQueueSize = 0);
    ~ThreadPool();

    void start();// 线程池开始运行
    void stop();// 线程池结束标记
    void addTask(Task_t task);// 添加任务

    int getThreadNum() const// 返回线程数
    { return threadNum_; }

private:
    bool starting_;// 线程池工作状态
    int threadNum_;// 线程池数量
    int maxQueueSize_;// 队列的最大数量
    int queueSize_;// 当前队列中任务的数量
    std::vector<pthread_t> threads_;// 线程池数组
    std::vector<Task_t> taskQueue_;// 任务队列(采用环状结构)
    int front_;// 队列头
    int rear_;// 队列尾后元素
    ::pthread_mutex_t mutex_;// 线程池锁，用于锁住整个线程池 
    ::pthread_cond_t queueNotFull_;// 条件变量，用于通知任务队列可添加新任务
    ::pthread_cond_t queueNotEmpty_;// 条件变量，用于通知线程从任务队列取任务
};

#endif


