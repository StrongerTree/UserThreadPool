#include "cthreadpool.h"

#define COUNT 2

ThreadPool* threadPoolCreate(int min, int max, int taskMaxCount)
{
    printf("cthreadpool init..............\n");
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do
    {
        if (NULL == pool)
        {
            printf("threadpool malloc failed！\n");
            break;
        }
        //初始化线程
        pool->pThreadsWorking = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if (NULL == pool->pThreadsWorking)
        {
            printf("pThreadsWorking malloc failed！\n");
            break;
        }
        memset(pool->pThreadsWorking, 0, sizeof(pthread_t) * max);

        pool->limit_threadMinCount = min;
        pool->limit_threadMaxCount = max;
        pool->spy_threadWorkingCount = 0;
        pool->spy_threadAllCount = min;    // 初始化线程数量和最小个数相等
        pool->spy_threadExitCount = 0;

        pthread_create(&pool->threadManager, NULL, managerSpying, pool);
        for (int i = 0; i < min; ++i)
        {
            pthread_create(&pool->pThreadsWorking[i], NULL, working, pool);
            printf("+++++++++++++初始化创建子线程, ID: %ld\n",pool->pThreadsWorking[i]);
        }
        //初始化锁与条件变量
        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexWorking, NULL) != 0 ||
            pthread_cond_init(&pool->condEmpty, NULL) != 0 ||
            pthread_cond_init(&pool->condFull, NULL) != 0)
        {
            printf("mutex or condition init failed！\n");
            break;
        }

        // 初始化任务队列
        pool->pTaskQueue = (Task*)malloc(sizeof(Task) * taskMaxCount);
        pool->limit_taskMaxCount = taskMaxCount;
        pool->spy_taskCount = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->isPoolShut = 0;

        return pool;
    } while (0);

    // 如初始化失败
    if (pool && pool->pThreadsWorking) free(pool->pThreadsWorking);
    if (pool && pool->pTaskQueue) free(pool->pTaskQueue);
    if (pool) free(pool);

    return NULL;
}

int threadPoolDestroy(ThreadPool* pool)
{
    if (NULL == pool)
    {
        return -1;
    }
    //线程及任务队列的回收
    pool->isPoolShut = 1;
    pthread_join(pool->threadManager, NULL);
    for (int i = 0; i < pool->spy_threadAllCount; ++i)
    {
        pthread_cond_signal(&pool->condEmpty);
    }
    sleep(60);  //延时一下，要不pool->pThreadsWorking会被free
    if (pool->pTaskQueue)
    {
        free(pool->pTaskQueue); pool->pTaskQueue = NULL;
    }
    if (pool->pThreadsWorking)
    {
        free(pool->pThreadsWorking); pool->pThreadsWorking = NULL;
    }
    //锁与条件变量资源回收
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexWorking);
    pthread_cond_destroy(&pool->condEmpty);
    pthread_cond_destroy(&pool->condFull);

    free(pool); pool = NULL;

    return 0;
}


void threadPoolAddTask(ThreadPool* pool, void(*func)(void*), void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    while (pool->spy_taskCount == pool->limit_taskMaxCount && !pool->isPoolShut)
    {
        pthread_cond_wait(&pool->condFull, &pool->mutexPool);
    }
    if (pool->isPoolShut)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // 添加任务
    pool->pTaskQueue[pool->queueRear].pTaskFunc = func;
    pool->pTaskQueue[pool->queueRear].taskArg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->limit_taskMaxCount;
    pool->spy_taskCount++;

    pthread_cond_signal(&pool->condEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

int threadCountWorking(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexWorking);
    int threadWorkingCount = pool->spy_threadWorkingCount;
    pthread_mutex_unlock(&pool->mutexWorking);
    return threadWorkingCount;
}

int threadCountAll(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int threadAllCount = pool->spy_threadAllCount;
    pthread_mutex_unlock(&pool->mutexPool);
    return threadAllCount;
}

void* working(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;

    while (1)  // 循环工作
    {
        pthread_mutex_lock(&pool->mutexPool);
        while (pool->spy_taskCount == 0 && !pool->isPoolShut)
        {
            pthread_cond_wait(&pool->condEmpty, &pool->mutexPool);

            if (pool->spy_threadExitCount > 0)
            {
                pool->spy_threadExitCount--;
                if (pool->spy_threadAllCount > pool->limit_threadMinCount)
                {
                    pool->spy_threadAllCount--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }

        if (pool->isPoolShut)  //判断线程池是否被关闭
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        Task task;
        task.pTaskFunc = pool->pTaskQueue[pool->queueFront].pTaskFunc;
        task.taskArg = pool->pTaskQueue[pool->queueFront].taskArg;
        pool->queueFront = (pool->queueFront + 1) % pool->limit_taskMaxCount;
        pool->spy_taskCount--; //少一个任务了
        pthread_cond_signal(&pool->condFull);
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexWorking);
        pool->spy_threadWorkingCount++;  //多一个正在工作的线程
        pthread_mutex_unlock(&pool->mutexWorking);
        task.pTaskFunc(task.taskArg);  //执行任务
        free(task.taskArg);
        task.taskArg = NULL;
        pthread_mutex_lock(&pool->mutexWorking);
        pool->spy_threadWorkingCount--;  //执行完任务，少一个正在工作的线程
        pthread_mutex_unlock(&pool->mutexWorking);
    }
    return NULL;
}

void* managerSpying(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    while (!pool->isPoolShut)
    {
        sleep(3);

        pthread_mutex_lock(&pool->mutexPool);
        int taskCount = pool->spy_taskCount;
        int threadAllCount = pool->spy_threadAllCount;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexWorking);
        int threadWorkingCount = pool->spy_threadWorkingCount;
        pthread_mutex_unlock(&pool->mutexWorking);

         // 新建线程
        if (taskCount > threadAllCount && threadAllCount < pool->limit_threadMaxCount)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->limit_threadMaxCount && counter < COUNT
                && pool->spy_threadAllCount < pool->limit_threadMaxCount; ++i)
            {
                if (pool->pThreadsWorking[i] == 0)
                {
                    pthread_create(&pool->pThreadsWorking[i], NULL, working, pool);
                    printf("+++++++++++++管理者创建子线程, ID: %ld, 当前线程总数： %d\n", pool->pThreadsWorking[i], pool->spy_threadAllCount + 1);
                    counter++;
                    pool->spy_threadAllCount++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }
        // 销毁线程
        if (threadWorkingCount * 2 < threadAllCount && threadAllCount > pool->limit_threadMinCount)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->spy_threadExitCount = COUNT;
            pthread_mutex_unlock(&pool->mutexPool);
            for (int i = 0; i < COUNT; ++i)
            {
                pthread_cond_signal(&pool->condEmpty);
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPool* pool)
{
    pthread_t id = pthread_self();
    for (int i = 0; i < pool->limit_threadMaxCount; ++i)
    {
        if (pool->pThreadsWorking[i] == id)
        {
            pool->pThreadsWorking[i] = 0;
            break;
        }
    }
    printf("----------线程退出，thread ID： %ld\n", id);
    pthread_exit(NULL);
}
