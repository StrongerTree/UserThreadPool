#include "cppthreadpool.h"
/*******************任务队列*********************/
TaskQueue::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

TaskQueue::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}

void TaskQueue::addTask(Task& task)
{
    pthread_mutex_lock(&m_mutex);
    m_queue.push(task);
    pthread_mutex_unlock(&m_mutex);
}

void TaskQueue::addTask(TaskFunc func, void* taskArg)
{
    pthread_mutex_lock(&m_mutex);
    Task task;
    task.pTaskFunc = func;
    task.taskArg = taskArg;
    m_queue.push(task);
    pthread_mutex_unlock(&m_mutex);
}

Task TaskQueue::takeTask()
{
    pthread_mutex_lock(&m_mutex);
    Task task;
    if (m_queue.size() > 0)
    {
        task = m_queue.front();
        m_queue.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return task;
}
/*******************线程池*********************/
ThreadPool::ThreadPool(int minCnt, int maxCnt)
{
    cout << "cppthreadpool init.............." << endl;
    pTaskQueue = new TaskQueue;   
    //初始化线程
    limit_threadMinCount = minCnt;
    limit_threadMaxCount = maxCnt;
    spy_threadWorkingCount = 0;
    spy_threadAllCount = minCnt;

    pThreadsWorking = new pthread_t[maxCnt];
    if (pThreadsWorking == nullptr)
    {
        cout << "new pthread_t数组失败...." << endl;;
        exit(EXIT_FAILURE);
    }
    memset(pThreadsWorking, 0, sizeof(pthread_t) * maxCnt);

    //创建线程
    for (int i = 0; i < minCnt; ++i)
    {
        pthread_create(&pThreadsWorking[i], NULL, working, this);
        cout << "+++++++++++++初始化创建子线程, ID: " << to_string(pThreadsWorking[i]) << endl;
    }
    pthread_create(&m_threadManager, NULL, managerSpying, this);
    //线程同步初始化
    if (pthread_mutex_init(&m_mutexPool, NULL) != 0 ||
        pthread_cond_init(&m_CondEmpty, NULL) != 0)
    {
        cout << "线程同步初始化失败..." << endl;
        exit(EXIT_FAILURE);
    }
}


ThreadPool::~ThreadPool()
{
    isPoolShut = 1;
    pthread_join(m_threadManager, NULL);    // 销毁管理者线程

    for (int i = 0; i < spy_threadAllCount; ++i)
    {
        pthread_cond_signal(&m_CondEmpty);    // 唤醒并销毁工作线程
    }
    sleep(60);
    if (pTaskQueue) delete pTaskQueue;
    if (pThreadsWorking) delete[]pThreadsWorking;

    pthread_mutex_destroy(&m_mutexPool);
    pthread_cond_destroy(&m_CondEmpty);
}

void ThreadPool::addTask(Task task)
{
    if (isPoolShut)
    {
        return;
    }
    pTaskQueue->addTask(task);
    pthread_cond_signal(&m_CondEmpty);
}

int ThreadPool::getCountThreadAll()
{
//    pthread_mutex_lock(&m_mutexPool);
    int threadNum = 0;
    threadNum = spy_threadAllCount;
//    pthread_mutex_unlock(&m_mutexPool);
    return threadNum;
}

int ThreadPool::getCountThreadWorking()
{
    pthread_mutex_lock(&m_mutexPool);
    int busyNum = 0;
    busyNum = spy_threadWorkingCount;
    pthread_mutex_unlock(&m_mutexPool);
    return busyNum;
}

int ThreadPool::getCountTask()
{
    return this->pTaskQueue->size();
}

// 工作线程任务函数
void* ThreadPool::working(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (true)    // 循环工作
    {
        pthread_mutex_lock(&pool->m_mutexPool);
        while (pool->getCountTask() == 0 && !pool->isPoolShut)
        {
            pthread_cond_wait(&pool->m_CondEmpty, &pool->m_mutexPool);

            if (pool->spy_threadExitCount > 0)
            {
                pool->spy_threadExitCount--;
                if (pool->spy_threadAllCount > pool->limit_threadMinCount)
                {
                    pool->spy_threadAllCount--;
                    pthread_mutex_unlock(&pool->m_mutexPool);
                    pool->threadExit();
                }
            }
        }
        if (pool->isPoolShut)
        {
            pthread_mutex_unlock(&pool->m_mutexPool);
            pool->threadExit();
        }

        Task task = pool->pTaskQueue->takeTask();
        pool->spy_threadWorkingCount++;
        pthread_mutex_unlock(&pool->m_mutexPool);
        task.pTaskFunc(task.taskArg);
        pthread_mutex_lock(&pool->m_mutexPool);
        pool->spy_threadWorkingCount--;
        pthread_mutex_unlock(&pool->m_mutexPool);
    }
    return nullptr;
}


// 管理者线程任务函数
void* ThreadPool::managerSpying(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while (!pool->isPoolShut)
    {
        sleep(3);

        pthread_mutex_lock(&pool->m_mutexPool);
        int taskCnt = pool->getCountTask();
        int threadAll = pool->spy_threadAllCount;
        int threadWorking = pool->spy_threadWorkingCount;
        pthread_mutex_unlock(&pool->m_mutexPool);

        const int COUNT = 2;
        // 创建线程
        if (taskCnt > threadAll && threadAll < pool->limit_threadMaxCount)
        {
            pthread_mutex_lock(&pool->m_mutexPool);
            int num = 0;
            for (int i = 0; i < pool->limit_threadMaxCount && num < COUNT
                && pool->spy_threadAllCount < pool->limit_threadMaxCount; ++i)
            {
                if (pool->pThreadsWorking[i] == 0)
                {
                    pthread_create(&pool->pThreadsWorking[i], NULL, working, pool);
                    cout << "+++++++++++++管理者创建子线程, ID: " << to_string(pool->pThreadsWorking[i])   << "，当前线程总数：" << pool->getCountThreadAll() + 1 << endl;
                    num++;
                    pool->spy_threadAllCount++;
                }
            }
            pthread_mutex_unlock(&pool->m_mutexPool);
        }

        // 销毁线程
        if (threadWorking * 2 < threadAll && threadAll > pool->limit_threadMinCount)
        {
            pthread_mutex_lock(&pool->m_mutexPool);
            pool->spy_threadExitCount = COUNT;
            pthread_mutex_unlock(&pool->m_mutexPool);
            for (int i = 0; i < COUNT; ++i)
            {
                pthread_cond_signal(&pool->m_CondEmpty);
            }
        }
    }
    return nullptr;
}

void ThreadPool::threadExit()
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < limit_threadMaxCount; ++i)
    {
        if (pThreadsWorking[i] == tid)
        {

            pThreadsWorking[i] = 0;
            break;
        }
    }
    cout << "-----------线程退出，thread ID： " << to_string(pthread_self()) << ", 等待......" <<endl;
//    sleep(2);
    pthread_exit(NULL);
}
