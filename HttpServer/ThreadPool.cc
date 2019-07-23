#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include "ThreadPool.h" 
#include "CurrentThread.h"
//#include <sys/time.h>
using std::cout;
using std::endl;
/* 构造函数 */
ThreadPool::ThreadPool(unsigned threadNum, unsigned maxQueueSize)
    : starting_(false), 
      threadNum_(threadNum), 
      maxQueueSize_(maxQueueSize),
      queueSize_(0),
      threads_(),
      taskQueue_(maxQueueSize),
      front_(0),
      rear_(0),
      mutex_(),
      queueNotEmpty_(mutex_)
{
    for (int i = 0; i < threadNum_; ++i)
        threads_.emplace_back(std::bind(&ThreadPool::threadFunc, this));
}

ThreadPool::~ThreadPool()
{
    std::cout << "~ThreadPool " << std::endl;
    stop();
    for(int i = 0; i < threadNum_; ++i) 
    {
        threads_[i].join();
    }
}

void ThreadPool::start() 
{
    starting_ = true;
    /*启动min_thr_num个工作线程*/
    for (int i = 0; i < threadNum_; i++) 
    {
        //创建线程
        threads_[i].start();
	    cout << "start worker thread " << i << endl;
    }
}

void ThreadPool::stop()
{
    starting_ = false;
    //pthread_cond_wait(&queueNotEmpty_, &mutex_);
}

/*向线程池的任务队列中添加一个任务*/
int ThreadPool::addTask(Task_t task)
{
    /*如果线程池处于关闭状态*/
    if (!starting_) 
    {
        throw std::runtime_error("thread pool is closed, cannot add task!");
    }
    if (queueSize_ == maxQueueSize_) 
    {
        return -1;// 表示线程池任务数满
    }
    {
        MutexLockGuard guard(mutex_);
        /*添加任务到任务队列*/
        cout << "add task!" << endl;

        taskQueue_[rear_] = task; 
        rear_ = (rear_ + 1) % maxQueueSize_;  /* 逻辑环  */
        queueSize_++; //任务数加1
    }
    /*添加完任务后,队列就不为空了,唤醒线程池中的一个线程*/
    queueNotEmpty_.notify();
}

void ThreadPool::threadFunc()
{
   // std::cout << "worker thread is running :" << tid << std::endl;
    while (true) {
        Task_t empty;
        Task_t task;
        {// 临界区
            MutexLockGuard guard_(mutex_);
            //无任务则阻塞在，有任务则跳出
            while ((queueSize_ == 0) && starting_)// 为防止虚假唤醒，需要用循环
            { 
                queueNotEmpty_.wait();
            } 
            // struct timeval tv_begin, tv_end;
            // gettimeofday(&tv_begin, NULL);
            // 根据线程开关状态选择是否释放线程
            if (!starting_) 
            { //关闭线程池
                cout << "thread " <<CurrentThread::tid() << "is exiting";
                pthread_exit(NULL); //线程自己结束自己
            }
            //否则该线程可以拿出任务
            task = taskQueue_[front_]; //出队操作
            taskQueue_[front_] = empty;
            front_ = (front_ + 1) % maxQueueSize_;  //环型结构
            queueSize_--;
        } 
        //执行刚才取出的任务
        cout << "ThreadPool do task" << endl;
        task();                           //执行任务
//        gettimeofday(&tv_end, NULL);
//        std::cout << "process time = " << tv_end.tv_sec - tv_begin.tv_sec << "s " 
//                  << tv_end.tv_usec - tv_begin.tv_usec << "us" << std::endl;
        //任务结束处理
       // printf("thread 0x%x end working \n", (unsigned int)pthread_self());
    }
    pthread_exit(NULL);
}



// 测试
// class Test
// {
// private:
//     /* data */
// public:
//     Test(/* args */);
//     ~Test();
//     void func()
//     {
//         std::cout << "Work in thread: " << std::this_thread::get_id() << std::endl;
//     }
// };

// Test::Test(/* args */)
// {
// }

// Test::~Test()
// {
// }


// int main()
// {
//     Test t;
//     ThreadPool tp(2);
//     tp.Start();
//     for (int i = 0; i < 1000; ++i)
//     {
//         std::cout << "addtask" << std::this_thread::get_id() << std::endl;
//         tp.AddTask(std::bind(&Test::func, &t));
//     }
// }

