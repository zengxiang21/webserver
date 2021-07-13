
#ifndef _LOCKER_H
#define _LOCKER_H
//线程同步机制封装类
#include<pthread.h>
#include<semaphore.h>
#include<exception>
using namespace std;

class Locker{
public:
    Locker()
    {
        if(pthread_mutex_init(&m_mutex,NULL)){
            throw exception(); 
        }
    }
    ~Locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        pthread_mutex_lock(&m_mutex);
    }
    bool unlock()
    {
        pthread_mutex_unlock(&m_mutex);

    }
    pthread_mutex_t* getlock()
    {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

class Cond
{
private:
    pthread_cond_t m_cond;
    
public:
    Cond()
    {
        if(pthread_cond_init(&m_cond,NULL))
        {
            throw exception();
        }
    }
    ~Cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool conWait(pthread_mutex_t* mutex)
    {
        return pthread_cond_wait(&m_cond,mutex)==0;

    }
    bool conTimewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t)==0;
    }
    bool conSignal()
    {
        return pthread_cond_signal(&m_cond)==0;
    }
    bool conBroadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
};

class Sem
{
private:
    sem_t m_sem;
public:
    Sem()
    {
        if(sem_init(&m_sem,0,0)!=0){
            throw exception();
        }
    }
    Sem(int val)
    {
        if(sem_init(&m_sem,0,val)!=0){
            throw exception();
        }
    }
    ~Sem()
    {
        sem_destroy(&m_sem);
    }
    bool semWait()
    {
        return sem_wait(&m_sem)==0;
    }
    bool semPost()
    {
        return sem_post(&m_sem)==0;
    }
};
#endif




