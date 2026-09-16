#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace NativeEngine::Core {

using JobTask = std::function<void()>;

struct JobCounter {
    std::atomic<uint32_t> value{ 0 };

    void Increment() { value.fetch_add(1, std::memory_order_relaxed); }
    void Decrement() { value.fetch_sub(1, std::memory_order_release); }
    bool IsCompleted() const { return value.load(std::memory_order_acquire) == 0; }
    void Wait() const {
        while (!IsCompleted()) {
            std::this_thread::yield();
        }
    }
};

class ThreadSafeJobQueue {
public:
    ThreadSafeJobQueue() = default;

    void Push(JobTask task) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Tasks.push_back(std::move(task));
    }

    bool Pop(JobTask& task) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Tasks.empty()) {
            return false;
        }
        task = std::move(m_Tasks.front());
        m_Tasks.erase(m_Tasks.begin());
        return true;
    }

private:
    std::mutex m_Mutex;
    std::vector<JobTask> m_Tasks;
};

class JobSystem {
public:
    static JobSystem& GetInstance();

    void Initialize(uint32_t threadCount = 0);
    void Shutdown();

    void Submit(JobTask task, JobCounter* counter = nullptr);
    void DispatchBatch(uint32_t count, uint32_t groupSize, std::function<void(uint32_t index)> jobFunc, JobCounter* counter = nullptr);

    uint32_t GetWorkerThreadCount() const { return static_cast<uint32_t>(m_Workers.size()); }
    uint64_t GetExecutedTaskCount() const { return m_ExecutedTasks.load(); }

private:
    JobSystem() = default;
    ~JobSystem();

    void WorkerLoop(uint32_t threadIndex);

    std::vector<std::thread> m_Workers;
    std::vector<ThreadSafeJobQueue> m_PerThreadQueues;
    std::atomic<bool> m_Running{ false };
    std::atomic<uint32_t> m_NextQueueIdx{ 0 };
    std::atomic<uint64_t> m_ExecutedTasks{ 0 };

    std::mutex m_FallbackMutex;
    std::condition_variable m_CV;
};

} // namespace NativeEngine::Core
