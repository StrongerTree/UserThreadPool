# **线程池**

<font size=3,font color=gray>分支master：C++版线程池，g++ main.cpp cppthreadpool.cpp -lpthread<br> 分支threadpool_baseon_c：c语言版线程池。gcc main.c cthreadpool.c -lpthread<br>注：仅供有缘人参考。挺久前做的，稍整理后上传，希望无bug。</font>

____

## 1. 需求与问题分析

​        简单使用多个单线程可以解决相对固定的少量并发问题，但是如果并发任务数量较多且时机不确定，那么，创建与销毁线程随机且频繁。使用线程池即是为了自动管理线程与并发的任务。其达到的核心作用有：

* 线程的相对复用。线程并不是执行完一个任务立即销毁，而是分配另一个任务继续执行，直到任务数量少于一定量时再通知销毁。
* 自动取出并执行任务。

**当然线程池带来的线程安全问题需要特别小心处理。**

## 2. 线程池4个部分的分析

* 任务队列：存储任务，提供接口添加和销毁任务。
* 工作线程：线程池中的线程数量是不确定的，根据和任务数对比来动态增减，作用是循环取出任务并处理任务。
* 管理者线程：对比任务数和线程数，动态创建与销毁线程。
*  线程池：管理任务、线程和整个线程池的运行。

**从封装思想看，每个部分负责各自的任务，需要区分管理者和线程池，管理者线程这一角色偏重监控，线程池这一角色偏重管理。**

### (1) 任务和任务队列

* **任务：**工作任务，函数，执行任务的过程就是调用函数的过程。
* **任务队列：**工作列表，容器，先入先出，后入后出。包括参数：当前总的任务数，最大任务数量，提供接口添加和删除任务。

**C语言：**

```c
//c 
//1. 任务结构体，包含一个函数指针和函数参数。
    typedef struct Task{
         void* (*pTaskFunc)(void*);
         void* taskArg;
    }Task;
//2. 任务队列
	Task *pTaskQueue;
//3. 任务队列的有关参数，为线程池ThreadPool的成员
    int limit_taskMaxCount;  // 限制：最多任务
    int spy_taskCount;       // 监视：当前任务数量
    int queueFront;          // 队首：取任务
    int queueRear;           // 队尾：加任务
//4. 任务队列初始化
//5. 往任务队列添加任务：针对队尾
//6. 往任务队列取任务：针对对首
//7. 任务队列释放资源    
    if (pool->pTaskQueue)
    {
        free(pool->pTaskQueue); pool->pTaskQueue = NULL;
    }
```

**! 每个线程都在随机对任务队列的操作，故需注意同步。**

**C++表示**

````c++
/* C++与C的不同即是封装，将任务和任务队列分别封装成类，看起来更加清晰明了 */
/* 1. 定义任务类，有两个成员：任务函数指针和参数指针,该参数其实应该升级为模板 */
/* .h */
using TaskFunc = void (*)(void*);
struct Task{
    Task():pTaskFunc(nullptr),taskArg(nullptr){}
    Task(TaskFunc func, void* arg):pTaskFunc(func),taskArg(arg){}
    TaskFunc pTaskFunc;
    void* pTaskArg;
}
/* 2. 定义任务队列类，保存任务，添加任务，提取任务，返回当前任务数量，内部进行线程同步 */
class TaskQueue{
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
}
/* .cpp */
TaskQueue::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);  //注意构造需要对锁进行初始化
}

TaskQueue::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);    //注意析构需要释放锁资源
}
/* 3. 线程池内部工作线程处理任务部分 */
    Task task = pool->pTaskQueue->takeTask();
    task.pTaskFunc(task.taskArg); //执行任务

````

### (2）工作线程

​        工作线程，主要负责处理任务，线程池维护着一定数量的线程数，这些线程是动态的，根据任务数量而变化。

**C语言**：

````c
/* 1. 工作线程为线程池成员， 这是一个线程组 */
	pthread_t *pThreadWorking;
/* 2. 工作线程相关参数 */
    pthread_t *pThreadsWorking;
    int limit_threadMinCount;  
    int limit_threadMaxCount;  
    //用spy太合适了
    int spy_threadAllCount;     
    int spy_threadWorkingCount; 
    int spy_threadExitCount;     //收到管理者线程通知应退出的线程
/* 3. 工作线程及其相关参数初始化 */
/* 4. 回收线程 */
/* 5. 线程工作函数*/

````

**C++表示**：

````c++
//1. 线程池维护一个工作线程组，包括创建，运行，销毁
pthread_t* pThreadsWorking;
/* 2. 工作线程相关参数，为线程池的监控成员，这几个参数很重要	 */
//限制：线程池最大和最小工作线程数量
int limit_threadMinCount;   
int limit_threadMaxCount;   
//监视线程池监视着工作线程数量：用spy太合适了
int spy_threadAllCount;      //当前线程总数，
int spy_threadWorkingCount;  //当前正在工作的线程数，
int spy_threadExitCount;     //收到管理者线程通知应退出的线程
/* 3. 创建工作线程及其相关参数初始化 */
limit_threadMinCount = minCnt;
limit_threadMaxCount = maxCnt;
spy_threadWorkingCount = 0;
spy_threadAllCount = minCnt;  //初始化线程数等于最小线程数
/* 4. 回收线程 */
/* 5. 工作线程函数*/
void* ThreadPool::working(void* arg)
{
    //（1）while(true)循环，不断从任务队列拿到任务，处理任务，不断的工作
    //（2）判断任务队列是否为空，线程池是否已关闭
   while (pool->getCountTask() == 0 && !pool->isPoolShut) {...}
   if (pool->isPoolShut){...}
    //（3）取任务，任务数该-1，正在工作的程数+1
    //（4）处理任务，处理完毕后正在工作的程数-1
}  
````

### (3) 管理者线程

​      管理者并不需要处理任务，其根据线程数量和任务数量，动态创建和销毁线程。

C++代码：

````c++
/* 1. 管理者线程 */
    pthread_t m_threadManager;   
/* 2. 创建管理者线程 */
    pthread_create(mThreadManager, NULL, managerSpying, this);
/* 3. 回收管理者线程资源 */
	pthread_join(m_threadManager, NULL);
/* 4. 管理者工作函数*/
void* ThreadPool::managerSpying(void* arg)
{
    //（1）while (!pool->isPoolShut)循环，每隔几秒判断一次线程池有没关闭，检测线程和任务参数
    //（2）获得当前总线程数和总任务数
    //（3）获得正在工作的线程数量
    //（4）进行判断：添加线程，当前的任务个数>当前总线程数 且 当前总线程数 < 最大线程数
        if (taskCnt > threadAll && threadAll < pool->limit_threadMaxCount) {...}
    // (5)进行判断：销毁线程：线程总数 > 工作线程数量的两倍 且 线程总数要大于最小线程数量限制
        if (threadWorking * 2 < threadAll && threadAll > pool->limit_threadMinCount)  {...}
}
````

### (4) 线程池

  线程池管理着整个线程池的运转，是最高管理者，其管理着一切信息，包括：任务队列及其参数（标志位）、工作中的线程及其参数、管理者线程、线程同步资源等。

**C语言：**

````c
//一、定义线程池
//二、创建线程池并初始化
//三、定义线程池相关函数：
//1. 销毁线程池
int threadPoolDestroy(ThreadPool* pool);
//2. 将任务添加至线程池
void threadPoolAddTask(ThreadPool* pool, void(*func)(void*), void* arg);
//3. 返回线程池的正在工作中线程数量
int threadCountWorking(ThreadPool* pool);
//4. 返回线程池的总线程数量，包括工作中和空闲线程
int threadCountAll(ThreadPool* pool);
//5. 工作线程函数
void* working(void* arg);
//6. 管理者线程函数
void* managerSpying(void* arg);
````

**C++表示：**

````c++
/* 1. 定义线程池类，其维护着一个任务队列，工作线程组，管理者线程，各个参数和线程同步 */
class ThreadPool
{
public:
    ...
    void addTask(Task task);  // 线程池添加任务
private:
    static void* working(void* arg);        // 工作线程的任务函数，参数为this
    static void* managerSpying(void* arg);  // 管理者线程的任务函数，参数为this
    
    void threadExit();  //线程退出

    pthread_t m_threadManager;   //管理者线程
    pthread_t* pThreadsWorking;  //工作线程相关...

    TaskQueue* pTaskQueue;//任务队列
    
    pthread_mutex_t m_mutexPool;   //线程同步
    pthread_cond_t m_CondEmpty; 
};
````







