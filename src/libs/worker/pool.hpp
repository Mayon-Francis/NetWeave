// #include "functional"
// #include "vector"
// #include "thread"
// #include "mutex"
// #include "condition_variable"
// #include "functional"

// template <typename funcType>
// class WorkerPool
// {
// private:
//     std::vector<std::thread> workers;
//     std::vector<std::function<void()>> tasks;
//     std::mutex tasks_mutex;
//     std::condition_variable tasks_cv;
//     bool stop;

// public:
//     WorkerPool(int num_workers);
//     ~WorkerPool();
//     void add_task(funcType task);
// };