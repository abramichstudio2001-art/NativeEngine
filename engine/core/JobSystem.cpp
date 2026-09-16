#include "engine/core/JobSystem.hpp"
#include "engine/core/Log.hpp"
#include <chrono>

namespace NativeEngine::Core {

JobSystem& JobSystem::GetInstance() {
    static JobSystem instance;
    return instance;
}

JobSystem::~JobSystem() {
    if (m_Running) {
        Shutdown();
    }
}

void JobSystem::Initialize(uint32_t threadCount) {
    if (m_Running) return;

    if (threadCount == 0) {
        uint32_t hw = std::thread::hardware_concurrency();
        threadCount = hw > 1 ? hw - 1 : 1;
    }

    m_Running = true;
    m_PerThreadQueues = std::vector<ThreadSafeJobQueue>(threadCount);

    for (uint32_t i = 0; i < threadCount; ++i) {
        m_Workers.emplace_back([this, i]() {
            WorkerLoop(i);
        });
    }

    NE_LOG_INFO("JobSystem", "Initialized JobSystem with {} worker threads", threadCount);
}

void JobSystem::Shutdown() {
    if (!m_Running) return;

    m_Running = false;
    m_CV.notify_all();

    for (auto& worker : m_Workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    m_Workers.clear();
    m_PerThreadQueues.clear();
    NE_LOG_INFO("JobSystem", "JobSystem shutdown complete");
}

void JobSystem::Submit(JobTask task, JobCounter* counter) {
    if (counter) {
        counter->Increment();
    }

    auto wrappedTask = [task = std::move(task), counter, this]() {
        task();
        if (counter) {
            counter->Decrement();
        }
        m_ExecutedTasks.fetch_add(1, std::memory_order_relaxed);
    };

    uint32_t targetQueue = m_NextQueueIdx.fetch_add(1, std::memory_order_relaxed) % m_Workers.size();
    m_PerThreadQueues[targetQueue].Push(std::move(wrappedTask));
    m_CV.notify_one();
}

void JobSystem::DispatchBatch(uint32_t count, uint32_t groupSize, std::function<void(uint32_t index)> jobFunc, JobCounter* counter) {
    if (count == 0) return;

    uint32_t groupCount = (count + groupSize - 1) / groupSize;
    for (uint32_t g = 0; g < groupCount; ++g) {
        uint32_t startIdx = g * groupSize;
        uint32_t endIdx = std::min(startIdx + groupSize, count);

        Submit([jobFunc, startIdx, endIdx]() {
            for (uint32_t i = startIdx; i < endIdx; ++i) {
                jobFunc(i);
            }
        }, counter);
    }
}

void JobSystem::WorkerLoop(uint32_t threadIndex) {
    const size_t queueCount = m_PerThreadQueues.size();

    while (m_Running) {
        JobTask task;
        bool foundTask = false;

        // Try own queue
        if (m_PerThreadQueues[threadIndex].Pop(task)) {
            foundTask = true;
        } else {
            // Work stealing from other queues safely
            for (size_t i = 1; i < queueCount; ++i) {
                size_t stealIdx = (threadIndex + i) % queueCount;
                if (m_PerThreadQueues[stealIdx].Pop(task)) {
                    foundTask = true;
                    break;
                }
            }
        }

        if (foundTask) {
            task();
        } else {
            std::unique_lock<std::mutex> lock(m_FallbackMutex);
            m_CV.wait_for(lock, std::chrono::microseconds(100), [this]() {
                return !m_Running;
            });
        }
    }
}

} // namespace NativeEngine::Core
