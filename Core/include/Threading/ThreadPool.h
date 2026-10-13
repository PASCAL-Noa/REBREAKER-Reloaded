#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <memory>
#include <type_traits>
#include <stdexcept>

class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Enqueues a task and returns a std::future for its result.
     */
    template <typename F, typename... Args>
    auto Enqueue(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f), ...capturedArgs = std::forward<Args>(args)]() mutable {
                return std::invoke(std::move(func), std::move(capturedArgs)...);
            }
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop.load(std::memory_order_acquire))
            {
                throw std::runtime_error("[ThreadPool] Enqueue called on stopped ThreadPool");
            }

            m_tasks.emplace([task]() {
                (*task)();
            });
            m_pendingCount.fetch_add(1, std::memory_order_relaxed);
        }

        m_condition.notify_one();
        return result;
    }

    /**
     * @brief Enqueues a detached task (fire-and-forget), avoiding std::packaged_task overhead.
     */
    void EnqueueDetached(std::function<void()> task);

    /**
     * @brief Blocks until all currently queued and executing tasks have completed.
     */
    void WaitAll();

    /**
     * @brief Signals workers to stop and joins all threads.
     */
    void Stop();

    [[nodiscard]] size_t GetPendingTasks() const noexcept;
    [[nodiscard]] size_t GetActiveTasks() const noexcept;
    [[nodiscard]] size_t GetWorkerCount() const noexcept;
    [[nodiscard]] bool   IsRunning() const noexcept;

private:
    void WorkerLoop();

    std::vector<std::thread>          m_workers;
    std::queue<std::function<void()>> m_tasks;

    mutable std::mutex               m_queueMutex;
    std::condition_variable          m_condition;
    std::condition_variable          m_finishedCondition;

    std::atomic<bool>                m_stop{false};
    std::atomic<size_t>              m_pendingCount{0};
    std::atomic<size_t>              m_activeCount{0};
};
