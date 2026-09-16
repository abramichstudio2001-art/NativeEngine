#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>

namespace NativeEngine::Core {

enum class AllocationCategory {
    General,
    EngineCore,
    Renderer,
    UI,
    Scripting,
    Audio,
    Physics
};

struct AllocationRecord {
    void* address;
    size_t size;
    size_t alignment;
    AllocationCategory category;
    uint64_t timestampUs;
    const char* tag;
};

struct HeapMetrics {
    size_t totalAllocatedBytes{ 0 };
    size_t totalFreedBytes{ 0 };
    size_t activeAllocations{ 0 };
    size_t peakAllocatedBytes{ 0 };
    size_t categoryBytes[8]{ 0 };
};

class MemoryTracker {
public:
    static MemoryTracker& GetInstance();

    void RecordAllocation(void* ptr, size_t size, size_t alignment, AllocationCategory category, const char* tag = nullptr);
    void RecordDeallocation(void* ptr);

    HeapMetrics GetMetrics() const;
    std::vector<AllocationRecord> GetRecentTimeline(size_t maxCount = 1000) const;

private:
    MemoryTracker() = default;

    mutable std::mutex m_Mutex;
    HeapMetrics m_Metrics;
    std::vector<AllocationRecord> m_Records;
};

class Allocator {
public:
    virtual ~Allocator() = default;
    virtual void* Allocate(size_t size, size_t alignment = 16, AllocationCategory cat = AllocationCategory::General, const char* tag = nullptr) = 0;
    virtual void Free(void* ptr) = 0;
};

class LinearAllocator : public Allocator {
public:
    explicit LinearAllocator(size_t totalSize);
    ~LinearAllocator() override;

    void* Allocate(size_t size, size_t alignment = 16, AllocationCategory cat = AllocationCategory::General, const char* tag = nullptr) override;
    void Free(void* ptr) override; // No-op for linear allocator
    void Reset();

    size_t GetUsedMemory() const { return m_Offset; }
    size_t GetTotalMemory() const { return m_TotalSize; }

private:
    uint8_t* m_StartPtr{ nullptr };
    size_t m_TotalSize{ 0 };
    size_t m_Offset{ 0 };
};

class ArenaAllocator : public Allocator {
public:
    explicit ArenaAllocator(size_t blockSize = 1024 * 1024);
    ~ArenaAllocator() override;

    void* Allocate(size_t size, size_t alignment = 16, AllocationCategory cat = AllocationCategory::General, const char* tag = nullptr) override;
    void Free(void* ptr) override;
    void Reset();

private:
    struct Block {
        uint8_t* memory;
        size_t size;
        size_t offset;
    };

    size_t m_BlockSize;
    std::vector<Block> m_Blocks;
    size_t m_CurrentBlockIdx{ 0 };
};

void* DynamicAllocate(size_t size, size_t alignment = 16, AllocationCategory cat = AllocationCategory::General, const char* tag = nullptr);
void DynamicFree(void* ptr);

} // namespace NativeEngine::Core
