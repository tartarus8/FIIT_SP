#include "../include/allocator_global_heap.h"
#include <stdexcept>
#include <new>
#include <cstddef>

allocator_global_heap::allocator_global_heap()
{
}

allocator_global_heap::~allocator_global_heap()
{
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(size_t size)
{
    size_t actual_size = (size == 0) ? 1 : size;
    size_t header_size = alignof(std::max_align_t);

    std::lock_guard<std::mutex> lock(_mutex);
    size_t total_size = actual_size + header_size;
    void* raw_memory = ::operator new(total_size);
    *reinterpret_cast<size_t*>(raw_memory) = size;

    return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(raw_memory) + header_size);
}

void allocator_global_heap::do_deallocate_sm(void *at)
{
    if (at == nullptr)
    {
        return;
    }

    size_t header_size = alignof(std::max_align_t);
    std::lock_guard<std::mutex> lock(_mutex);

    void* raw_memory = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(at) - header_size);

    ::operator delete(raw_memory);
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other)
    : allocator_dbg_helper(other)
    , smart_mem_resource(other)
{
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    if (this == &other)
    {
        return *this;
    }

    allocator_dbg_helper::operator=(other);
    smart_mem_resource::operator=(other);

    return *this;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept
    : allocator_dbg_helper(std::move(other))
    , smart_mem_resource(std::move(other))
{
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    allocator_dbg_helper::operator=(std::move(other));
    smart_mem_resource::operator=(std::move(other));
    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return dynamic_cast<const allocator_global_heap*>(&other) != nullptr;
}
