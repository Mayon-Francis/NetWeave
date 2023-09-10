#include "iostream"

#define enable_debug 1

#include "debug_log/debug.cpp"
#include "worker/pool.cpp"

int testShort(int n)
{
    for (int i = 0; i < n; i++)
    {
        std::cout << i << " ";
    }
    return n;
}
int testLong (int n)
{
    for (int i = 0; i < n*10000; i++)
        ;
}

int main()
{
    int workerCount = std::thread::hardware_concurrency() / 2;
    WorkerPool<int(int n), int, int> pool(workerCount);

    for (int i = 0; i < workerCount + 1; i++)
    {
        if (i % 2 == 0)
        {
            debug("Task Long %d added\n", i);
            pool.add_task(&testLong, 23);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        else
        {
            debug("Task Short %d added\n", i);
            pool.add_task(&testShort, 23);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    return 0;
}