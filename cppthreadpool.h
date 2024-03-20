#ifndef CPPTHREADPOOL_H
#define CPPTHREADPOOL_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
 #include <string.h>
#include <unistd.h>

#include <iostream>
#include <queue>
using namespace std;

using TaskFunc = void(*)(void*);
struct Task
{
    Task():pTaskFunc(nullptr), taskArg(nullptr) { };
    Task(TaskFunc func, void* arg):pTaskFunc(func), taskArg(arg) { };
    TaskFunc pTaskFunc;
    void* taskArg;
};

class TaskQueue
{
public:
    TaskQueue();
    ~TaskQueue();

    void addTask(Task& task);
    void addTask(TaskFunc func, void* taskArg);
    Task takeTask();
    inline int size()  { return m_queue.size(); }

private:
    pthread_mutex_t m_mutex;    // 队列内部已进行同步
    std::queue<Task> m_queue;
};

class ThreadPool
{
public:
    ThreadPool(int minCnt, int maxCnt);
    ~ThreadPool();

    void addTask(Task task);
    int getCountThreadWorking();
    int getCountThreadAll();
    int getCountTask();

private:
    static void* working(void* arg);        // 参数为this
    static void* managerSpying(void* arg);
    void threadExit();

private:
    pthread_t m_threadManager;   //管理者线程
    pthread_t* pThreadsWorking;  //工作线程
    int limit_threadMinCount;
    int limit_threadMaxCount;
    //用spy太合适了
    int spy_threadAllCount;
    int spy_threadWorkingCount;
    int spy_threadExitCount;

    TaskQueue* pTaskQueue;    //任务队列

    bool isPoolShut = false;

    pthread_mutex_t m_mutexPool;        //线程同步
    pthread_cond_t m_CondEmpty;
};



#endif // CPPTHREADPOOL_H


