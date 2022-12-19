#ifndef _THREADPOOL_H  /*防止重复包含*/
#define _THREADPOOL_H

typedef struct ThreadPool ThreadPool;

/*创建线程池*/
ThreadPool* threadPoolCreate(int min, int max, int queueSize);

/*销毁线程池*/
int threadPoolDestroy(ThreadPool* pool);

/*添加任务给线程池*/
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);  /*参数三是参数二的参数*/

/*忙碌线程数*/
int threadPoolBusyNum(ThreadPool* pool);

/*存活线程数*/
int threadPoolAliveNum(ThreadPool* pool);

/*工作相关*/
void* worker(void* arg);

/*管理者*/
void* manager(void* arg);

/*退出*/
void threadExit(ThreadPool* pool);

#endif // !_THREADPOOL_H

