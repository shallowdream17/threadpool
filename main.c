#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

void taskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is working, number = %d\n",
        pthread_self(), num);
    sleep(1);
}

int main()
{
    // 创建线程池
    threadpool* pool = threadpoolcreate(10, 30, 100);
    for (int i = 0; i < 201; i++)
    {
        int* num = (int*)malloc(sizeof(int));
        *num = i+100;
        threadpooladd(pool, taskFunc, num);
    }

    sleep(30);//假装在做别的事

    //printf("--------------------------------\n");
    //printf("%d\n",pool->livenum);
    //printf("%ld\n",pool->managerid);
    threadpooldestroy(pool);
    return 0;
}

