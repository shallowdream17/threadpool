#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"

//创建线程+初始化
threadpool *threadpoolcreate(int minnum,int maxnum,int queuecapacity){
    //申请堆内存
    threadpool *pool= (threadpool*)malloc(sizeof(threadpool));
    if(!pool){
        printf("Thread pool memory application failed...\n");
        return NULL;
    }
    pool->taskqueue=(task*)malloc(sizeof(task)*queuecapacity);
    if(!pool->taskqueue){
        printf("Failed to apply for thread pool task queue memory...\n");
        free(pool);
        pool=NULL;
        return NULL;
    }
    pool->workerids= (pthread_t*)malloc(sizeof(pthread_t)*maxnum);
    if(!pool->workerids){
        printf("Worker thread id memory application failed...\n");
        free(pool->taskqueue);
        pool->taskqueue=NULL;
        free(pool);
        pool=NULL;
        return NULL;
    }
    memset(pool->workerids,0,sizeof(pthread_t)*maxnum);

    //初始化锁和信号量
    if(pthread_mutex_init(&pool->mutexpool,NULL)!=0 ||
        pthread_mutex_init(&pool->mutexbusy,NULL)!=0 ||
        sem_init(&pool->full,0,0)!=0 ||
        sem_init(&pool->empty,0,queuecapacity)!=0){
        printf("mutex or sem init fail...\n");
        free(pool->workerids);//若初始化失败则释放之前申请的内存
        pool->workerids=NULL;
        free(pool->taskqueue);
        pool->taskqueue=NULL;
        free(pool);
        pool=NULL;
        return NULL;
    }


    //初始化各项属性
    pool->queuecapacity=queuecapacity;
    pool->queuesize=0;
    pool->queuefront=0;
    pool->queuetail=0;
    pool->minnum=minnum;
    pool->maxnum=maxnum;
    pool->busynum=0;
    pool->livenum=minnum;
    pool->exitnum=0;

    pool->shutdown=0;


    //创建管理者线程和工作线程
    pthread_create(&pool->managerid,NULL,managerfunc,pool);
    for(int i=0;i<minnum;i++){//创建minnum个最小线程数量
        printf("created thread...\n");
        pthread_create(&pool->workerids[i],NULL,workerfunc,pool);
    }
    return pool;
}

//销毁线程池
int threadpooldestroy(threadpool *pool){
    if(pool==NULL){//线程池为空则销毁失败
        return -1;
    }
    printf("Thread pool destruction start...\n");
    pool->shutdown=1;//关闭线程池
    pthread_join(pool->managerid,NULL);//阻塞回收管理者线程
    pthread_mutex_lock(&pool->mutexpool);
    int livenum=pool->livenum;
    pthread_mutex_unlock(&pool->mutexpool);
    for(int i=0;i<livenum;i++){
        sem_post(&pool->full);
    }
    while(1){
        pthread_mutex_lock(&pool->mutexpool);
        int livenum=pool->livenum;
        pthread_mutex_unlock(&pool->mutexpool);
        if(livenum>0){
            sleep(1);
        }
        else{
            break;
        }
    }
    if(pool->taskqueue!=NULL){
        free(pool->taskqueue);
        pool->taskqueue=NULL;
    }
    if(pool->workerids!=NULL){
        free(pool->workerids);
        pool->workerids=NULL;
    }

    pthread_mutex_destroy(&pool->mutexpool);
    pthread_mutex_destroy(&pool->mutexbusy);
    sem_destroy(&pool->full);
    sem_destroy(&pool->empty);

    free(pool);
    pool=NULL;
    return 0;

}

//添加任务
void threadpooladd(threadpool *pool,void (*function)(void*),void *arg){
    sem_wait(&pool->empty);
    pthread_mutex_lock(&pool->mutexpool);
    if(pool->shutdown==1){
        pthread_mutex_unlock(&pool->mutexpool);
        sem_post(&pool->empty);
        return;
    }
    pool->taskqueue[pool->queuetail].function=function;
    pool->taskqueue[pool->queuetail].arg=arg;
    pool->queuetail=(pool->queuetail+1)%pool->queuecapacity;
    pool->queuesize++;

    pthread_mutex_unlock(&pool->mutexpool);
    sem_post(&pool->full);

}

//获取线程池中工作的线程的个数
int threadpoolbusynum(threadpool *pool){
    pthread_mutex_lock(&pool->mutexpool);
    int ans=pool->busynum;
    pthread_mutex_unlock(&pool->mutexpool);
    return ans;
}

//获取线程池中活着的线程的个数
int thradpoollivenum(threadpool *pool){
    pthread_mutex_lock(&pool->mutexpool);
    int ans=pool->livenum;
    pthread_mutex_unlock(&pool->mutexpool);
    return ans;
}

//工作者
void *workerfunc(void *arg){
    threadpool *pool= (threadpool*)arg;
    while(1){
        sem_wait(&pool->full);
        pthread_mutex_lock(&pool->mutexpool);
        if(pool->exitnum>0){
            pool->exitnum--;
            if(pool->livenum>pool->minnum){
                pool->livenum--;
                pthread_mutex_unlock(&pool->mutexpool);
                sem_post(&pool->empty);
                threadexit(pool);
            }
        }
        if(pool->shutdown==1){
            pool->livenum--;
            pthread_mutex_unlock(&pool->mutexpool);
            sem_post(&pool->empty);
            threadexit(pool);
        }

        task *nowtask=(task*)malloc(sizeof(task));
        nowtask->function=pool->taskqueue[pool->queuefront].function;
        nowtask->arg=pool->taskqueue[pool->queuefront].arg;
        pool->queuefront=(pool->queuefront+1)%pool->queuecapacity;
        pool->queuesize--;

        //解锁
        pthread_mutex_unlock(&pool->mutexpool);
        sem_post(&pool->empty);

        printf("thread %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexbusy);
        pool->busynum++;
        pthread_mutex_unlock(&pool->mutexbusy);
        nowtask->function(nowtask->arg);
        free(nowtask);
        nowtask=NULL;

        printf("thread %ld end working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexbusy);
        pool->busynum--;
        pthread_mutex_unlock(&pool->mutexbusy);

    }
    return NULL;
}

//管理者
void *managerfunc(void *arg){
    threadpool *pool=(threadpool*)arg;

    while(pool->shutdown==0){//待修改
        sleep(3);

        pthread_mutex_lock(&pool->mutexpool);
        int queuesize=pool->queuesize;
        int livenum=pool->livenum;
        pthread_mutex_unlock(&pool->mutexpool);

        pthread_mutex_lock(&pool->mutexbusy);
        int busynum=pool->busynum;
        pthread_mutex_unlock(&pool->mutexbusy);

        //添加线程
        //任务的个数>存活的线程个数 && 存活的线程数<最大线程数
        if(queuesize>livenum&&livenum<pool->maxnum){
            pthread_mutex_lock(&pool->mutexpool);
            int counter=0;
            for (int i = 0; i < pool->maxnum && counter < 2 && pool->livenum < pool->maxnum; ++i){
                if(pool->workerids[i]==0){
                    printf("created thread...\n");
                    pthread_create(&pool->workerids[i],NULL,workerfunc,pool);
                    counter++;
                    pool->livenum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexpool);

        }

        //销毁线程
        //忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
        if(busynum*2<livenum && livenum > pool->minnum){
            pthread_mutex_lock(&pool->mutexpool);
            //pool->exitnum=2;
            int killnum=0;//记录要杀死的线程个数
            if(livenum-pool->minnum>=2){
                pool->exitnum=2;
                killnum=2;
            }
            else{
                pool->exitnum=livenum-pool->minnum;
                killnum=livenum-pool->minnum;
            }
            pthread_mutex_unlock(&pool->mutexpool);
            for(int i=0;i<killnum;i++){//杀死几个就唤醒几个
                sem_wait(&pool->empty);
                sem_post(&pool->full);
            }
        }
    }
    return NULL;
}

//单个工作者线程退出
void threadexit(threadpool *pool){
    pthread_t tid=pthread_self();
    for(int i=0;i<pool->maxnum;i++){
        if(pool->workerids[i]==tid){
            pool->workerids[i]=0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
