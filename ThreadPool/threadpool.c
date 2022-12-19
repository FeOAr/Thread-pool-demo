#include"threadpool.h"
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>  /*memset*/
#include<unistd.h> 
const int NUMBER = 2;

/*任务结构体*/
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
}Task, *TaskP;

/*线程池结构体*/
struct ThreadPool
{
	TaskP taskQueue;  //任务队列
	int queueCapacity;  // 总容量
	int queueSize;      // 当前任务个数
	int queueFront;     // 队头数组下标 -> 取数据
	int queueRear;      // 队尾数组下标 -> 放数据
	
    pthread_t managerID;    // 管理者线程ID
    pthread_t* threadIDs;   // 工作的线程ID，有多个，所以定义一个指针，指向工作者线程数组
    int minNum;             // 最小线程数量
    int maxNum;             // 最大线程数量
    int busyNum;            // 忙的线程的个数
    int liveNum;            // 存活的线程的个数
    int exitNum;            // 要销毁的线程个数
    pthread_mutex_t mutexPool;  // 锁整个的线程池
    pthread_mutex_t mutexBusy;  // 锁busyNum变量
    pthread_cond_t fullSignal;     // 任务队列是不是满了
    pthread_cond_t emptySignal;    // 任务队列是不是空了

    int shutdown;           // 是不是要销毁线程池, 销毁为1, 不销毁为0
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));  /*实例化了一个主体*/
    do
    {
        if (pool == NULL)  /*分配失败*/
        {
            printf("malloc threadpool fail..");
            break;
        }
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);  /*做多同时工作线程是max个，所以按照最大申请*/
        if (pool->threadIDs == NULL)
        {
            printf("malloc threadIDs fail..");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);  /*用0区分是否可以存线程ID*/
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;  /*初始和最小数量相等*/
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 || pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->emptySignal, NULL) != 0 || pthread_cond_init(&pool->fullSignal, NULL) != 0)  /*函数成功完成之后会返回零，返回其他则错误*/
        {
            printf("mutex or condition init fail...\n");
            break;
        }

        /*任务队列*/
        pool->taskQueue = (TaskP)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        /*创建线程*/
        /*创建管理者线程；最后一位为回调函数传入参数*/
        pthread_create(&pool->managerID, NULL, manager, pool);  
        /*此行运行后，则会创建一个管理者线程*/
        for (int i = 0; i < min; i++)
        {
            /*此行运行后，则会创建一个工作者线程，按照规则自行实现消费者功能*/
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);  /*创建最小工作线程*/
        }
        return pool;
    } while (0);  /*为了使用break，所以用while*/

    /*如果失败，break到此处，释放资源*/
    if (pool && pool->threadIDs) free(pool->threadIDs);
    if (pool) free(pool);
    return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
    if (pool == NULL)
    {
        return -1;
    }

    // 关闭线程池
    pool->shutdown = 1;
    // 阻塞回收管理者线程
    pthread_join(pool->managerID, NULL);
    // 唤醒阻塞的消费者线程
    for (int i = 0; i < pool->liveNum; ++i)  /*循环次数：没有任务时销毁*/
    {
        pthread_cond_signal(&pool->emptySignal);
    }
    // 释放堆内存
    if (pool->taskQueue)
    {
        free(pool->taskQueue);
    }
    if (pool->threadIDs)
    {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->emptySignal);
    pthread_cond_destroy(&pool->fullSignal);

    free(pool);
    pool = NULL;
    return 0;
}

/*生产者功能实现*/
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    /*如果任务池满并且没关闭*/
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        // 阻塞生产者线程
        pthread_cond_wait(&pool->fullSignal, &pool->mutexPool);  
        /*第一个参数用来唤醒，第二个参数用来临时释放互斥锁避免死锁*/
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // 添加任务
    pool->taskQueue[pool->queueRear].function = func;  /*任务队列里的队尾，该项函数指针赋值*/
    pool->taskQueue[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;  /*按照循环队列的规则，尾指针后移一位*/
    pool->queueSize++;  /*任务队列当前大小*/

    pthread_cond_signal(&pool->emptySignal);  /*释放非空信号，唤醒因为空阻塞的线程*/
    pthread_mutex_unlock(&pool->mutexPool);  /*对pool操作完毕，释放互斥锁*/
}

int threadPoolBusyNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}

void* worker(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;  /*给worker传的参数是pool*/
    while (1)
    {
        pthread_mutex_lock(&pool->mutexPool);  /*线程池加锁*/
        /*当前队列为空还没关？*/
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            /*工作线程阻塞，唤醒后若有多个，则仍旧抢mutex互斥锁，没抢到继续阻塞，只有一个会抢到*/
            pthread_cond_wait(&pool->emptySignal, &pool->mutexPool);
            /*是否销毁线程，和manager的销毁唤醒有关*/
            if (pool->exitNum > 0)
            {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum)  /*多一道保险*/
                {
					pool->liveNum--;
					/* Q:如果在 manger 线程取出 liveNum 时，worker 线程执行了销毁操作，那之前取到的
					 * liveNum 的值是不是和当前真正的值不一样了?
					 * A:管理者线程 3 秒轮训一次，从第一次算，在管理者没有发送自杀信号之前，其他工作线
					 * 程是不会销毁的，只有当管理者给 pool->exitNum 赋值后，工作线程才从阻塞中被唤醒，
					 * 并进行销毁操作。在这之后，管理者线程又睡眠 3s，3s 完全足够工作线程销毁了，如果
					 * 3s 内工作线程都没有销毁，直到下次管理线程继续判断是否要销毁线程，而造成上下层不
					 * 一致，这个已经属于系统异常了
					 */
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }
        /*从任务队列中取出一个任务*/
        Task task;
        task.function = pool->taskQueue[pool->queueFront].function;  /*从任务队列头部取一个任务*/
        task.arg = pool->taskQueue[pool->queueFront].arg;
        /*移动头节点*/
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;  /*环形队列*/
        pool->queueSize--;
        pthread_cond_signal(&pool->fullSignal);  /*唤醒生产者*/
        pthread_mutex_unlock(&pool->mutexPool);  /*至此，取任务结束*/

        /*---------------开始工作--------------*/
        printf("thread %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;  /*繁忙线程 + 1*/
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg);  /*函数指针方式调用；执行函数指针指向的函数并传入参数*/
        /*(*task.function)(task.arg) ， 常规解引用后，函数方式调用*/
        free(task.arg);  /*pool->taskQueue[pool->queueFront].arg 是堆区创建的*/
        task.arg = NULL;

        printf("thread %ld end working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void* manager(void* arg)
{
	ThreadPool* pool = (ThreadPool*)arg;
	while (!pool->shutdown)
	{
		// 每隔3s检测一次
		sleep(3);

		// 取出线程池中任务的数量和当前线程的数量
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// 取出忙的线程的数量
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// 添加线程
		// 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
		if (queueSize > liveNum && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;  /*增加线程的计数器*/
			for (int i = 0; i < pool->maxNum && counter < NUMBER
				&& pool->liveNum < pool->maxNum; ++i)
			{
				if (pool->threadIDs[i] == 0)  /*从0开始找可以存ID的项*/
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
		// 销毁线程
		// 忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// 让工作的线程自杀
			for (int i = 0; i < NUMBER; ++i)
			{
                /*因为每次只会唤醒一个，遂循环两次*/
				pthread_cond_signal(&pool->emptySignal);  /*销毁空闲阻塞的工作线程*/
			}
		}
	}
	return NULL;
}

void threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i)
    {
        if (pool->threadIDs[i] == tid)  //找到自己线程ID所在的位置
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
