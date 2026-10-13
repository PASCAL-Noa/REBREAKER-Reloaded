#include "Threading/ThreadPool.h"
#include <algorithm>
#include <iostream>

ThreadPool::ThreadPool(size_t threadCount)
{
    if (threadCount == 0)
    {
        unsigned int hw = std::thread::hardware_concurrency();
        threadCount = std::max(2u, hw);
    }

    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i)
    {
        m_workers.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

ThreadPool::~ThreadPool()
{
    Stop();
}

void ThreadPool::WorkerLoop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this]() {
                return m_stop.load(std::memory_order_acquire) || !m_tasks.empty();
            });

            if (m_stop.load(std::memory_order_acquire) && m_tasks.empty())
            {
                return;
            }

            if (!m_tasks.empty())
            {
                task = std::move(m_tasks.front());
                m_tasks.pop();
                m_pendingCount.fetch_sub(1, std::memory_order_relaxed);
                m_activeCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if (task)
        {
            try
            {
                task();
            }
            catch (const std::exception& e)
            {
                std::cerr << "[ThreadPool] Exception in worker task: " << e.what() << '\n';
            }
            catch (...)
            {
                std::cerr << "[ThreadPool] Unknown exception in worker task\n";
            }

            m_activeCount.fetch_sub(1, std::memory_order_release);

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                if (m_tasks.empty() && m_activeCount.load(std::memory_order_acquire) == 0)
                {
                    m_finishedCondition.notify_all();
                }
            }
        }
    }
}

void ThreadPool::EnqueueDetached(std::function<void()> task)
{
    if (!task) return;

    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_stop.load(std::memory_order_acquire))
        {
            return;
        }

        m_tasks.emplace(std::move(task));
        m_pendingCount.fetch_add(1, std::memory_order_relaxed);
    }

    m_condition.notify_one();
}

void ThreadPool::WaitAll()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_finishedCondition.wait(lock, [this]() {
        return m_tasks.empty() && m_activeCount.load(std::memory_order_acquire) == 0;
    });
}

void ThreadPool::Stop()
{
    bool expected = false;
    if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return; // Already stopped
    }

    m_condition.notify_all();

    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    m_workers.clear();
}

size_t ThreadPool::GetPendingTasks() const noexcept
{
    return m_pendingCount.load(std::memory_order_relaxed);
}

size_t ThreadPool::GetActiveTasks() const noexcept
{
    return m_activeCount.load(std::memory_order_relaxed);
}

size_t ThreadPool::GetWorkerCount() const noexcept
{
    return m_workers.size();
}

bool ThreadPool::IsRunning() const noexcept
{
    return !m_stop.load(std::memory_order_relaxed);
}
