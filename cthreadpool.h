#ifndef _CTHREADPOOL_H
#define _CTHREADPOOL_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
 #include <string.h>
#include <unistd.h>

// 任务结构体
typedef struct Task
{
    void (*pTaskFunc)(void* arg);
    void* taskArg;
}Task;

// 线程池结构体
struct ThreadPool
{
    // 任务队列
    Task* pTaskQueue;
    int limit_taskMaxCount;
    int spy_taskCount;
    int queueFront;
    int queueRear;

    pthread_t threadManager;    // 管理者线程
    pthread_t *pThreadsWorking; // 工作线程
    int limit_threadMinCount;   
    int limit_threadMaxCount;  
    //用spy太合适了
    int spy_threadAllCount;      
    int spy_threadWorkingCount;  
    int spy_threadExitCount;     

    //线程同步
    pthread_mutex_t mutexPool; 
    pthread_mutex_t mutexWorking;
    pthread_cond_t condFull;    
    pthread_cond_t condEmpty;    

    int isPoolShut;           // 销毁线程池
};

typedef struct ThreadPool ThreadPool;

ThreadPool *threadPoolCreate(int min, int max, int taskMaxCount);
int threadPoolDestroy(ThreadPool* pool);
void threadPoolAddTask(ThreadPool* pool, void(*func)(void*), void* arg);
void threadExit(ThreadPool* pool);
int threadCountWorking(ThreadPool* pool);
int threadCountAll(ThreadPool* pool);
void* working(void* arg);
void* managerSpying(void* arg);

#endif  // _CTHREADPOOL_H



