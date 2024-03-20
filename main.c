#include "cthreadpool.h"

void taskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("正在处理任务，thread ID: %ld, num = %d\n", pthread_self(), num);
    sleep(1);
}

int main()
{
    ThreadPool* pool = threadPoolCreate(3, 10, 100);
    for (int i = 0; i < 120; ++i)
    {
        int* num = (int*)malloc(sizeof(int));  //内部释放
        *num = i;
        threadPoolAddTask(pool, taskFunc, num);
    }

    sleep(60);
    threadPoolDestroy(pool);
    sleep(60);

    return 0;
}

