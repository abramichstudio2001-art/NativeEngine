#include "engine/core/Memory.hpp"
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace NativeEngine::Core {

static size_t AlignUp(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

MemoryTracker& MemoryTracker::GetInstance() {
    static MemoryTracker instance;
    return instance;
}

void MemoryTracker::RecordAllocation(void* ptr, size_t size, size_t alignment, AllocationCategory category, const char* tag) {
    if (!ptr) return;

    auto now = std::chrono::system_clock::now();
    uint64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Metrics.totalAllocatedBytes += size;
    m_Metrics.activeAllocations++;
    size_t currentActive = m_Metrics.totalAllocatedBytes - m_Metrics.totalFreedBytes;
    if (currentActive > m_Metrics.peakAllocatedBytes) {
        m_Metrics.peakAllocatedBytes = currentActive;
    }

    size_t catIdx = static_cast<size_t>(category);
    if (catIdx < 8) {
        m_Metrics.categoryBytes[catIdx] += size;
    }

    m_Records.push_back(AllocationRecord{
        .address = ptr,
        .size = size,
        .alignment = alignment,
        .category = category,
        .timestampUs = nowUs,
        .tag = tag ? tag : "Unassigned"
    });

    if (m_Records.size() > 10000) {
        m_Records.erase(m_Records.begin(), m_Records.begin() + 1000);
    }
}

void MemoryTracker::RecordDeallocation(void* ptr) {
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = std::find_if(m_Records.rbegin(), m_Records.rend(), [ptr](const AllocationRecord& rec) {
        return rec.address == ptr;
    });

    if (it != m_Records.rend()) {
        m_Metrics.totalFreedBytes += it->size;
        if (m_Metrics.activeAllocations > 0) m_Metrics.activeAllocations--;
        size_t catIdx = static_cast<size_t>(it->category);
        if (catIdx < 8 && m_Metrics.categoryBytes[catIdx] >= it->size) {
            m_Metrics.categoryBytes[catIdx] -= it->size;
        }
    }
}

HeapMetrics MemoryTracker::GetMetrics() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Metrics;
}

std::vector<AllocationRecord> MemoryTracker::GetRecentTimeline(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Records.size() <= maxCount) {
        return m_Records;
    }
    return std::vector<AllocationRecord>(m_Records.end() - maxCount, m_Records.end());
}

// Linear Allocator
LinearAllocator::LinearAllocator(size_t totalSize) : m_TotalSize(totalSize) {
    size_t allocSize = AlignUp(totalSize, 16);
    m_StartPtr = static_cast<uint8_t*>(std::aligned_alloc(16, allocSize));
    m_Offset = 0;
}

LinearAllocator::~LinearAllocator() {
    if (m_StartPtr) {
        std::free(m_StartPtr);
        m_StartPtr = nullptr;
    }
}

void* LinearAllocator::Allocate(size_t size, size_t alignment, AllocationCategory cat, const char* tag) {
    uintptr_t currentAddr = reinterpret_cast<uintptr_t>(m_StartPtr + m_Offset);
    uintptr_t alignedAddr = (currentAddr + (alignment - 1)) & ~(alignment - 1);
    size_t padding = alignedAddr - currentAddr;

    if (m_Offset + padding + size > m_TotalSize) {
        return nullptr;
    }

    m_Offset += padding + size;
    void* ptr = reinterpret_cast<void*>(alignedAddr);
    MemoryTracker::GetInstance().RecordAllocation(ptr, size, alignment, cat, tag);
    return ptr;
}

void LinearAllocator::Free(void* ptr) {
    // No individual free for linear allocator
    (void)ptr;
}

void LinearAllocator::Reset() {
    m_Offset = 0;
}

// Arena Allocator
ArenaAllocator::ArenaAllocator(size_t blockSize) : m_BlockSize(blockSize) {
    size_t allocSize = AlignUp(m_BlockSize, 16);
    uint8_t* mem = static_cast<uint8_t*>(std::aligned_alloc(16, allocSize));
    m_Blocks.push_back(Block{ .memory = mem, .size = m_BlockSize, .offset = 0 });
}

ArenaAllocator::~ArenaAllocator() {
    for (auto& block : m_Blocks) {
        if (block.memory) {
            std::free(block.memory);
        }
    }
    m_Blocks.clear();
}

void* ArenaAllocator::Allocate(size_t size, size_t alignment, AllocationCategory cat, const char* tag) {
    if (m_Blocks.empty()) return nullptr;

    Block* currentBlock = &m_Blocks[m_CurrentBlockIdx];
    uintptr_t currentAddr = reinterpret_cast<uintptr_t>(currentBlock->memory + currentBlock->offset);
    uintptr_t alignedAddr = (currentAddr + (alignment - 1)) & ~(alignment - 1);
    size_t padding = alignedAddr - currentAddr;

    if (currentBlock->offset + padding + size > currentBlock->size) {
        size_t newSize = std::max(m_BlockSize, size + alignment);
        size_t allocSize = AlignUp(newSize, 16);
        uint8_t* mem = static_cast<uint8_t*>(std::aligned_alloc(16, allocSize));
        m_Blocks.push_back(Block{ .memory = mem, .size = newSize, .offset = 0 });
        m_CurrentBlockIdx = m_Blocks.size() - 1;
        currentBlock = &m_Blocks[m_CurrentBlockIdx];

        currentAddr = reinterpret_cast<uintptr_t>(currentBlock->memory);
        alignedAddr = (currentAddr + (alignment - 1)) & ~(alignment - 1);
        padding = alignedAddr - currentAddr;
    }

    currentBlock->offset += padding + size;
    void* ptr = reinterpret_cast<void*>(alignedAddr);
    MemoryTracker::GetInstance().RecordAllocation(ptr, size, alignment, cat, tag);
    return ptr;
}

void ArenaAllocator::Free(void* ptr) {
    // Arena frees on reset
    (void)ptr;
}

void ArenaAllocator::Reset() {
    for (auto& block : m_Blocks) {
        block.offset = 0;
    }
    m_CurrentBlockIdx = 0;
}

void* DynamicAllocate(size_t size, size_t alignment, AllocationCategory cat, const char* tag) {
    size_t allocSize = AlignUp(size, alignment);
    void* ptr = std::aligned_alloc(alignment, allocSize);
    MemoryTracker::GetInstance().RecordAllocation(ptr, size, alignment, cat, tag);
    return ptr;
}

void DynamicFree(void* ptr) {
    if (ptr) {
        MemoryTracker::GetInstance().RecordDeallocation(ptr);
        std::free(ptr);
    }
}

} // namespace NativeEngine::Core
