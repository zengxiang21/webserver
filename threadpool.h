#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include"locker.h"
#include "sql_connection_pool.h"
#include<queue>
#include<string.h>
#include<stdio.h>
using namespace std;
template<typename T>
class ThreadPool
{
private:
    /* data */
    int m_thread_num;//线程数量
    int m_max_requests;//任务队列中允许的最大请求数
    pthread_t* m_threadIDs;//线程池数组(工作队列)，大小为m_thread_num
    queue<T*> m_workQueue;//任务队列
    Locker m_queueLocker;//任务队列锁
    Sem m_queueStat;//任务队列信号量
    connection_pool* m_connPool;//线程池包含数据库池
    int m_actor_model;//模型切换
    static void* worker(void *arg)
    {
         ThreadPool* pool=static_cast<ThreadPool*>(arg);
         pool->getAndprocess();
         return pool;
         
    }
    void getAndprocess()
    {
         while(true)
         {
               //1.取任务队列中的一个任务
               //任务队列信号量--
               m_queueStat.semWait();
               //有就上锁
               m_queueLocker.lock();
               if(m_workQueue.empty())
               {
                    m_queueLocker.unlock();
                    continue;
               }
               //2.取任务
               T* request=m_workQueue.front();
               m_workQueue.pop();
               m_queueLocker.unlock();
               if(!request)
               {
                    continue;
               }
               
               if (1 == m_actor_model)//reactor模式
               {
                    //m_stat=0:读
                    if (0 == request->m_state)
                    {
                         if (request->read())
                         {
                              request->improv = 1;
                              connectionRAII mysqlcon(&request->mysql, m_connPool);
                              request->process();
                         }
                         else
                         {
                              request->improv = 1;
                              request->timer_flag = 1;
                         }
                    }
                    //m_stat=1:写
                    else
                    {
                         if (request->write())
                         {
                              request->improv = 1;
                         }
                         else
                         {
                              request->improv = 1;
                              request->timer_flag = 1;
                         }
                    }
               }
               //
               else
               {
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
               } 
         }

    }

public:
     ThreadPool(int actor_model, connection_pool *connPool,int thread_num=8,int max_requests=10000)
     {
          if(max_requests<0||thread_num<0)
          {
               throw exception();
          }
          m_max_requests=max_requests;
          m_thread_num=thread_num;
          m_threadIDs=nullptr;
          m_connPool=connPool;
          m_actor_model=actor_model;

          //创建线程
          m_threadIDs=new pthread_t[m_thread_num];
          memset(m_threadIDs,0,sizeof(pthread_t)*m_thread_num);
          if(!m_threadIDs)
          {
               throw exception();
          }
          //创建线程并设置线程分离
          for(int i=0;i<m_thread_num;i++)
          {
               printf("create the %dth thread\n",i);
               if(pthread_create(&m_threadIDs[i],NULL,worker,this)!=0)
               {
                    delete [] m_threadIDs;
                    throw exception();
               }
               if(pthread_detach(m_threadIDs[i])!=0)
               {
                    delete[] m_threadIDs;
                    throw exception();
               }

          }
          
     }
     ~ThreadPool()
     {
          delete []m_threadIDs;

     }

     bool append(T *request, int state)
     {
     m_queueLocker.lock();
     if (m_workQueue.size() >= m_max_requests)
     {
          m_queueLocker.unlock();
          return false;
     }
     request->m_state = state;
     m_workQueue.push(request);
     m_queueLocker.unlock();
     m_queueStat.semPost();
     return true;
     }
     //向任务队列中添加任务
     bool append_p(T* request)
     {
          m_queueLocker.lock();//锁上任务队列
          //判断一下任务队列
          if (m_workQueue.size()>m_max_requests)
          {
               m_queueLocker.unlock();
               return false;
          }
          m_workQueue.push(request);
          m_queueLocker.unlock();
          //生产了一个任务，任务队列信号量++
          m_queueStat.semPost();
          return true;
     }
};

#endif