#include"threadpool.h"
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>  /*memset*/
#include<unistd.h> 
const int NUMBER = 2;

/*����ṹ��*/
typedef struct Task
{
	void (*function)(void* arg);
	void* arg;
}Task, *TaskP;

/*�̳߳ؽṹ��*/
struct ThreadPool
{
	TaskP taskQueue;  //�������
	int queueCapacity;  // ������
	int queueSize;      // ��ǰ�������
	int queueFront;     // ��ͷ�����±� -> ȡ����
	int queueRear;      // ��β�����±� -> ������
	
    pthread_t managerID;    // �������߳�ID
    pthread_t* threadIDs;   // �������߳�ID���ж�������Զ���һ��ָ�룬ָ�������߳�����
    int minNum;             // ��С�߳�����
    int maxNum;             // ����߳�����
    int busyNum;            // æ���̵߳ĸ���
    int liveNum;            // �����̵߳ĸ���
    int exitNum;            // Ҫ���ٵ��̸߳���
    pthread_mutex_t mutexPool;  // ���������̳߳�
    pthread_mutex_t mutexBusy;  // ��busyNum����
    pthread_cond_t fullSignal;     // ��������ǲ�������
    pthread_cond_t emptySignal;    // ��������ǲ��ǿ���

    int shutdown;           // �ǲ���Ҫ�����̳߳�, ����Ϊ1, ������Ϊ0
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));  /*ʵ������һ������*/
    do
    {
        if (pool == NULL)  /*����ʧ��*/
        {
            printf("malloc threadpool fail..");
            break;
        }
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);  /*����ͬʱ�����߳���max�������԰����������*/
        if (pool->threadIDs == NULL)
        {
            printf("malloc threadIDs fail..");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);  /*��0�����Ƿ���Դ��߳�ID*/
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;  /*��ʼ����С�������*/
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 || pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->emptySignal, NULL) != 0 || pthread_cond_init(&pool->fullSignal, NULL) != 0)  /*�����ɹ����֮��᷵���㣬�������������*/
        {
            printf("mutex or condition init fail...\n");
            break;
        }

        /*�������*/
        pool->taskQueue = (TaskP)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        /*�����߳�*/
        /*�����������̣߳����һλΪ�ص������������*/
        pthread_create(&pool->managerID, NULL, manager, pool);  
        /*�������к���ᴴ��һ���������߳�*/
        for (int i = 0; i < min; i++)
        {
            /*�������к���ᴴ��һ���������̣߳����չ�������ʵ�������߹���*/
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);  /*������С�����߳�*/
        }
        return pool;
    } while (0);  /*Ϊ��ʹ��break��������while*/

    /*���ʧ�ܣ�break���˴����ͷ���Դ*/
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

    // �ر��̳߳�
    pool->shutdown = 1;
    // �������չ������߳�
    pthread_join(pool->managerID, NULL);
    // �����������������߳�
    for (int i = 0; i < pool->liveNum; ++i)  /*ѭ��������û������ʱ����*/
    {
        pthread_cond_signal(&pool->emptySignal);
    }
    // �ͷŶ��ڴ�
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

/*�����߹���ʵ��*/
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    /*��������������û�ر�*/
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        // �����������߳�
        pthread_cond_wait(&pool->fullSignal, &pool->mutexPool);  
        /*��һ�������������ѣ��ڶ�������������ʱ�ͷŻ�������������*/
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // �������
    pool->taskQueue[pool->queueRear].function = func;  /*���������Ķ�β�������ָ�븳ֵ*/
    pool->taskQueue[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;  /*����ѭ�����еĹ���βָ�����һλ*/
    pool->queueSize++;  /*������е�ǰ��С*/

    pthread_cond_signal(&pool->emptySignal);  /*�ͷŷǿ��źţ�������Ϊ���������߳�*/
    pthread_mutex_unlock(&pool->mutexPool);  /*��pool������ϣ��ͷŻ�����*/
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
    ThreadPool* pool = (ThreadPool*)arg;  /*��worker���Ĳ�����pool*/
    while (1)
    {
        pthread_mutex_lock(&pool->mutexPool);  /*�̳߳ؼ���*/
        /*��ǰ����Ϊ�ջ�û�أ�*/
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            /*�����߳����������Ѻ����ж�������Ծ���mutex��������û��������������ֻ��һ��������*/
            pthread_cond_wait(&pool->emptySignal, &pool->mutexPool);
            /*�Ƿ������̣߳���manager�����ٻ����й�*/
            if (pool->exitNum > 0)
            {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum)  /*��һ������*/
                {
					pool->liveNum--;
					/* Q:����� manger �߳�ȡ�� liveNum ʱ��worker �߳�ִ�������ٲ�������֮ǰȡ����
					 * liveNum ��ֵ�ǲ��Ǻ͵�ǰ������ֵ��һ����?
					 * A:�������߳� 3 ����ѵһ�Σ��ӵ�һ���㣬�ڹ�����û�з�����ɱ�ź�֮ǰ������������
					 * ���ǲ������ٵģ�ֻ�е������߸� pool->exitNum ��ֵ�󣬹����̲߳Ŵ������б����ѣ�
					 * ���������ٲ���������֮�󣬹������߳���˯�� 3s��3s ��ȫ�㹻�����߳������ˣ����
					 * 3s �ڹ����̶߳�û�����٣�ֱ���´ι����̼߳����ж��Ƿ�Ҫ�����̣߳���������²㲻
					 * һ�£�����Ѿ�����ϵͳ�쳣��
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
        /*�����������ȡ��һ������*/
        Task task;
        task.function = pool->taskQueue[pool->queueFront].function;  /*���������ͷ��ȡһ������*/
        task.arg = pool->taskQueue[pool->queueFront].arg;
        /*�ƶ�ͷ�ڵ�*/
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;  /*���ζ���*/
        pool->queueSize--;
        pthread_cond_signal(&pool->fullSignal);  /*����������*/
        pthread_mutex_unlock(&pool->mutexPool);  /*���ˣ�ȡ�������*/

        /*---------------��ʼ����--------------*/
        printf("thread %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;  /*��æ�߳� + 1*/
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg);  /*����ָ�뷽ʽ���ã�ִ�к���ָ��ָ��ĺ������������*/
        /*(*task.function)(task.arg) �� ��������ú󣬺�����ʽ����*/
        free(task.arg);  /*pool->taskQueue[pool->queueFront].arg �Ƕ���������*/
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
		// ÿ��3s���һ��
		sleep(3);

		// ȡ���̳߳�������������͵�ǰ�̵߳�����
		pthread_mutex_lock(&pool->mutexPool);
		int queueSize = pool->queueSize;
		int liveNum = pool->liveNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// ȡ��æ���̵߳�����
		pthread_mutex_lock(&pool->mutexBusy);
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexBusy);

		// ����߳�
		// ����ĸ���>�����̸߳��� && �����߳���<����߳���
		if (queueSize > liveNum && liveNum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			int counter = 0;  /*�����̵߳ļ�����*/
			for (int i = 0; i < pool->maxNum && counter < NUMBER
				&& pool->liveNum < pool->maxNum; ++i)
			{
				if (pool->threadIDs[i] == 0)  /*��0��ʼ�ҿ��Դ�ID����*/
				{
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					counter++;
					pool->liveNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
		// �����߳�
		// æ���߳�*2 < �����߳��� && �����߳�>��С�߳���
		if (busyNum * 2 < liveNum && liveNum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexPool);
			pool->exitNum = NUMBER;
			pthread_mutex_unlock(&pool->mutexPool);
			// �ù������߳���ɱ
			for (int i = 0; i < NUMBER; ++i)
			{
                /*��Ϊÿ��ֻ�ỽ��һ������ѭ������*/
				pthread_cond_signal(&pool->emptySignal);  /*���ٿ��������Ĺ����߳�*/
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
        if (pool->threadIDs[i] == tid)  //�ҵ��Լ��߳�ID���ڵ�λ��
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
