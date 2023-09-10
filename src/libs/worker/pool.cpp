#include "pool.hpp"
#include "vector"
#include "queue"
#include "thread"
#include "mutex"
#include "condition_variable"
#include "functional"
#include "type_traits"
#include "debug_log/debug.cpp"
#include "future"

template <class F, class R = std::result_of_t<F &()>, typename ...Args>
class WorkerPool
{

private:
    
    std::vector<std::thread> workers;
    std::queue<std::packaged_task<R()>> tasks;
    std::mutex tasks_mutex;
    std::condition_variable tasks_cv;
    bool stop;

    void workerLoop()
    {
        while (!stop)
        {
            std::unique_lock<std::mutex> lock(tasks_mutex);
            tasks_cv.wait(
                lock,
                [this]()
                { return (!tasks.empty() || stop); });

            // Even if taska are queued up on wake-up,
            // prioritize stopping if stop is requested
            if (stop)
            {
                debug("Thread %zu Stopping\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
                return;
            }

            if (!tasks.empty())
            {
                auto task = std::move(tasks.front());
                tasks.pop();

                // Unlock before executing the task
                // otherwise worker execution will not be parallel
                lock.unlock();
                debug("Thread %zu Starting task\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
                task();
                debug("Thread %zu Finished task\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
            }
        }
    }

public:
    WorkerPool(int num_workers)
    {
        // static_assert(std::is_function<funcType>::value, "Must be a function");
        workers.reserve(num_workers);

        for (int i = 0; i < num_workers; i++)
        {
            workers.emplace_back(std::thread(&WorkerPool::workerLoop, this));
        }
    }
    ~WorkerPool()
    {
        debug("Destructing\n");
        stop = true;
        tasks_cv.notify_all();
        for (auto &worker : workers)
        {
            worker.join();
        }
        debug("Destructed\n");
    }

    std::future<R> add_task(F task, Args... args)
    {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        std::packaged_task<R()> p(std::bind(task, args...));
        auto r = p.get_future();
        tasks.push(std::move(p));

        tasks_cv.notify_one();
        return r;
    }
};