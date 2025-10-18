#include "../../include/thread_pool.h"


namespace rpc {

ThreadPool::ThreadPool(size_t thread_count) 
    :stop_(false),
    active_threads_(0) 
{
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency(); // CPU核心数
        if (thread_count == 0) {
            thread_count = 1;
        }
    }
    // 创建工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&ThreadPool::workerThread, this);
    }

    std::cout << "ThreadPool created with " << thread_count << "threads" << std::endl;
}

ThreadPool::~ThreadPool() {
    stop();
}

// 等待所有任务完成
void ThreadPool::waitForAllTasks() {
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // 等待所有任务完成
    condition_.wait(lock, [this] { return tasks_.empty() && active_threads_ == 0; });
}
    
// 获取任务队列大小
size_t ThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

// 获取活跃现场数量
size_t ThreadPool::getActiveThreadCount() const {
    return active_threads_.load();
}

// 获取线程池大小
size_t ThreadPool::getThreadCount() const {
    return workers_.size();
}

// 线程池是否正在运行
bool ThreadPool::isRunning() const {
    return !stop_.load();
}

// 停止线程池
void ThreadPool::stop() {
    if (stop_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }

    condition_.notify_all();

    // 等待所有工作线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
    std::cout << "ThreadPool stopped" << std::endl;
}

// 关闭
void ThreadPool::shutdown() {
    stop();
}

// 主线程: 从任务队列取出任务，执行任务
void ThreadPool::workerThread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 等待任务
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front()); // 获取任务
            tasks_.pop(); // 移除任务
            active_threads_++;
        }

        // 执行任务
        task();
        // 执行完毕
        active_threads_--;
    }
}

}