#include<stdio.h>
#include"threadpool.h"
#include<pthread.h>
#include<unistd.h>  //sleep
#include<stdlib.h>

void taskFunc(void* arg)  /*添加的工作，具体行为*/
{
    int num = *(int*)arg;
    printf("thread %ld is working, number = %d\n", pthread_self(), num);
    sleep(1);
}

int main()
{
    // 创建线程池
    ThreadPool* pool = threadPoolCreate(3, 10, 100);  /*最小三个工作线程，最大10个，总计一百个*/
    for (int i = 0; i < 100; ++i)
    {
        int* num = (int*)malloc(sizeof(int));
        *num = i + 100;
        threadPoolAdd(pool, taskFunc, num);
    }

    sleep(30);  /*让主线程迟一点销毁线程池*/

    threadPoolDestroy(pool);
    return 0;
}
