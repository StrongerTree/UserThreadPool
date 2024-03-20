#include "cppthreadpool.h"

void taskFunc(void* arg)
{
    int num = *(int*)arg;
    cout << "正在处理任务，thread ID: " << to_string(pthread_self()) << ",num =：" << num << endl;
    sleep(1);
}

int main()
{
    ThreadPool pool(3, 10);
    int num[200];
    for (int i = 0; i < 200; ++i)
    {
        num[i] = i;
        Task task(taskFunc, &num[i]);  //连续创建200个task，每个task输出num数组元素值
        pool.addTask(task);
    }

    sleep(60);

    return 0;
}
