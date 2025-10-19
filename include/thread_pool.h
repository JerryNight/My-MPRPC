#pragma once

#include <memory>        // 智能指针相关头文件
#include <vector>        // 向量容器头文件
#include <queue>         // 队列容器头文件
#include <thread>        // 线程相关头文件
#include <mutex>         // 互斥锁头文件
#include <condition_variable>  // 条件变量头文件
#include <functional>    // 函数对象头文件
#include <atomic>        // 原子操作头文件
#include <stdexcept>     // 异常处理头文件
#include <future>
#include <iostream>



namespace rpc {

// 线程池
class ThreadPool {
public:
    ThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~ThreadPool();

    // 禁用拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务
    template<typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args) -> std::future<typename std::result_of<Func(Args...)>::type>;

    template<typename Func, typename... Args>
    auto submitVoid(Func&& func, Args&&... args) -> std::future<void>;

    // 等待所有任务完成
    void waitForAllTasks();
    
    // 获取任务队列大小
    size_t getQueueSize() const;

    // 获取活跃现场数量
    size_t getActiveThreadCount() const;

    // 获取线程池大小
    size_t getThreadCount() const;

    // 线程池是否正在运行
    bool isRunning() const;

    // 停止线程池
    void stop();

    // 优雅关闭
    void shutdown();
private:
    std::vector<std::thread> workers_; // 工作线程
    std::queue<std::function<void()>> tasks_; // 任务队列
    mutable std::mutex queue_mutex_; // 队列互斥锁
    std::condition_variable condition_; // 线程同步
    std::atomic<bool> stop_; // 停止标志
    std::atomic<size_t> active_threads_; // 活跃线程数量

    // 主线程: 从任务队列取出任务，执行任务
    void workerThread();
};

// 提交任务的函数模板实现
template<typename Func, typename... Args>
auto ThreadPool::submit(Func&& func, Args&&... args) -> std::future<typename std::result_of<Func(Args...)>::type> {
    if (stop_) {
        throw std::runtime_error("Cannot submit task to stopped thread pool");
    }

    // 创建任务包装器
    using ReturnType = typename std::result_of<Func(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

    // 获取future 对象
    std::future<ReturnType> result = task->get_future();

    // 将任务添加到队列中
    {
        std::unique_lock<std::mutex> lock(queue_mutex_); 
        tasks_.emplace([task]() { (*task)(); });
    }

    // 唤醒线程
    condition_.notify_one();

    // 返回 future 对象
    return result;
}

template<typename Func, typename... Args>
auto ThreadPool::submitVoid(Func&& func, Args&&... args) -> std::future<void>{
    if (stop_) {
        throw std::exception("Cannot submit task to stopped thread pool");
    }

    // 创建任务包装器
    auto task = std::make_shared<std::packaged_task<void()>>(  
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...)  
    );

    // 获取future对象
    std::future<void> result = task->get_future();

    // 将任务添加到队列中
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace([task]() { (*task)(); });
    }

    // 通知一个等待的线程
    condition_.notify_one(); 

    return result;  // 返回future对象
}

}