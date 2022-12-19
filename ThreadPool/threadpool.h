#ifndef _THREADPOOL_H  /*��ֹ�ظ�����*/
#define _THREADPOOL_H

typedef struct ThreadPool ThreadPool;

/*�����̳߳�*/
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

/*�����̳߳�*/
int threadPoolDestroy(ThreadPool* pool);

/*����������̳߳�*/
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);  /*�������ǲ������Ĳ���*/

/*æµ�߳���*/
int threadPoolBusyNum(ThreadPool* pool);

/*����߳���*/
int threadPoolAliveNum(ThreadPool* pool);

/*�������*/
void* worker(void* arg);

/*������*/
void* manager(void* arg);

/*�˳�*/
void threadExit(ThreadPool* pool);

#endif // !_THREADPOOL_H
