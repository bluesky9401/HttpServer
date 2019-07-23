
#ifndef _CONDITION_H_
#define _CONDITION_H_

#include <pthread.h>
#include <iostream>
#include "MutexLock.h"

class Condition : private Noncopyable 
{
public:
    explicit Condition(MutexLock& mutex)
        : mutex_(mutex)
    {
        if (0 != pthread_cond_init(&pcond_, NULL)) 
        {
            std::cout << "can't init condition " << std::endl;
        }
    }

    ~Condition()
    {
        pthread_cond_destroy(&pcond_);
    } 

    void wait()
    {
        pthread_cond_wait(&pcond_, mutex_.get());
    }

    void notify()
    {
        pthread_cond_signal(&pcond_);
    }

    void notifyAll()
    {
        pthread_cond_broadcast(&pcond_);
    }

private:
    MutexLock& mutex_;
    pthread_cond_t pcond_;
};
#endif
