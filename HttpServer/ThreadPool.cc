#include "ThreadPool.h" 
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <stdexcept>
/* 构造函数 */
ThreadPool::ThreadPool(unsigned threadNum, unsigned maxQueueSize)
    : starting_(false), threadNum_(threadNum), maxQueueSize_(maxQueueSize)
{
    queueSize_ = 0;
    threads_.resize(threadNum_);
    taskQueue_.resize(maxQueueSize_);
    rear_ = 0;
    front_ = 0;
    if ( pthread_mutex_init(&mutex_, NULL) != 0      ||
	     pthread_cond_init(&queueNotFull_, NULL) !=0 ||
         pthread_cond_init(&queueNotEmpty_, NULL) != 0) {
         throw std::runtime_error("init lock or cond false;");
      }
}

ThreadPool::~ThreadPool()
{
    std::cout << "~ThreadPool " << pthread_self() << std::endl;
    stop();
    // 销毁锁与条件变量
    pthread_mutex_lock(&mutex_);
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&queueNotEmpty_);
    pthread_cond_destroy(&queueNotFull_);

    for(int i = 0; i < threadNum_; ++i) {
        pthread_join(threads_[i], NULL);
    }
}

void ThreadPool::start() 
{
    starting_ = true;
    /*启动min_thr_num个工作线程*/
    for (int i=0; i<threadNum_; i++) {
        //创建线程
        pthread_create(&threads_[i], NULL, threadFunc, (void*)this);
	  //  printf("start worker thread 0x%x... \n", (unsigned int)threads_[i]);
    }
}

void ThreadPool::stop()
{
    starting_ = false;
    pthread_cond_wait(&queueNotEmpty_, &mutex_);
}

/*向线程池的任务队列中添加一个任务*/
void ThreadPool::addTask(Task_t task)
{
   pthread_mutex_lock(&mutex_);//访问条件变量前必须加锁

   /*如果队列满了,调用wait阻塞*/
   while ((queueSize_ == maxQueueSize_) && starting_) {
      pthread_cond_wait(&queueNotFull_, &mutex_);
   }

   /*如果线程池处于关闭状态*/
   if (!starting_) {
      pthread_mutex_unlock(&mutex_);
      throw std::runtime_error("thread pool is closed, cannot add task!");
   }
      
   /*添加任务到任务队列*/
   taskQueue_[rear_] = task; 
   rear_ = (rear_ + 1) % maxQueueSize_;  /* 逻辑环  */
   queueSize_++; //任务数加1

   /*添加完任务后,队列就不为空了,唤醒线程池中的一个线程*/
   pthread_cond_signal(&queueNotEmpty_);
   pthread_mutex_unlock(&mutex_);
}

void* threadFunc(void *args)
{
    ThreadPool *thisp = (ThreadPool*) args;
    pthread_t tid = pthread_self();
   // std::cout << "worker thread is running :" << tid << std::endl;
    ThreadPool::Task_t task;
    while (true) {
        pthread_mutex_lock(&(thisp->mutex_));

        //无任务则阻塞在，有任务则跳出
        while ((thisp->queueSize_ == 0) && thisp->starting_) { 
           // printf("thread 0x%x is waiting \n", (unsigned int)pthread_self());
            pthread_cond_wait(&(thisp->queueNotEmpty_), &(thisp->mutex_));
        } 

        // 根据线程开关状态选择是否释放线程
        if (!thisp->starting_) { //关闭线程池
            pthread_mutex_unlock(&(thisp->mutex_));
            printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
            pthread_exit(NULL); //线程自己结束自己
        }

        //否则该线程可以拿出任务
        task = thisp->taskQueue_[thisp->front_]; //出队操作

        thisp->front_ = (thisp->front_ + 1) % thisp->maxQueueSize_;  //环型结构
        thisp->queueSize_--;
        
        //通知可以添加新任务
        pthread_cond_broadcast(&(thisp->queueNotFull_));
    
        //释放线程锁
        pthread_mutex_unlock(&(thisp->mutex_));
    
        //执行刚才取出的任务
        task();                           //执行任务
        
        //任务结束处理
      //  printf("thread 0x%x end working \n", (unsigned int)pthread_self());
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

