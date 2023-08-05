#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <semaphore.h>

//任务结构体
typedef struct task{
    void (*function)(void *arg);//指向函数的指针，指向返回类型为void，且参数为一个void*类型的函数
    void *arg;
}task;

typedef struct threadpool{
    //任务队列
    task *taskqueue;
    int queuecapacity;//队列总容量
    int queuesize;//当前队列中的任务个数
    int queuefront;//队头
    int queuetail;//队尾

    pthread_t managerid;//管理者线程
    pthread_t *workerids;//工作线程
    int minnum;//最小线程数量
    int maxnum;//最大线程数量
    int busynum;//忙碌线程数量
    int livenum;//存活的线程数量
    int exitnum;//退出的线程数量
    pthread_mutex_t mutexpool;//锁下整个线程池
    pthread_mutex_t mutexbusy;//锁下忙碌的线程数量
    sem_t full;//任务队列中的任务个数
    sem_t empty;//任务队列中的空闲位置个数

    int shutdown;//是否要销毁线程

}threadpool;


//创建线程池   最小线程个数-最大线程个数-任务队列容量
threadpool* threadpoolcreate(int minnum,int maxnum,int queuecapacity);

//销毁线程池
int threadpooldestroy(threadpool* pool);
//给线程池添加任务
void threadpooladd(threadpool *pool,void (*function)(void*arg),void *arg);
//获取线程池中工作的线程个数
int threadpoolbusynum(threadpool *p);
//获取线程池中存活的线程个数
int threadpoollivenum(threadpool* p);

//工作者线程
void *workerfunc(void *arg);
//管理者线程
void *managerfunc(void *arg);
//单个线程退出
void threadexit(threadpool *p);

#endif //THREADPOOL_H

