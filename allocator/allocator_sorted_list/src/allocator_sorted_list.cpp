#include "../include/allocator_sorted_list.h"
#include <stdexcept>
#include <cstring>
#include <vector>
#include <mutex>
#include <new>

static inline std::mutex& get_mutex(void* trusted) {
    return *reinterpret_cast<std::mutex*>(trusted);
}
static inline std::pmr::memory_resource*& get_parent(void* trusted) {
    return *reinterpret_cast<std::pmr::memory_resource**>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::mutex));
}
static inline size_t& get_space_size(void* trusted) {
    return *reinterpret_cast<size_t*>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::mutex) + sizeof(std::pmr::memory_resource*));
}
static inline void*& get_first_free(void* trusted) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::mutex) + sizeof(std::pmr::memory_resource*) + sizeof(size_t));
}
static inline allocator_with_fit_mode::fit_mode& get_fit_mode(void* trusted) {
    return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::mutex) + sizeof(std::pmr::memory_resource*) + sizeof(size_t) + sizeof(void*));
}

static inline size_t& get_block_size(void* block) {
    return *reinterpret_cast<size_t*>(block);
}
static inline void*& get_next_free(void* block) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(block) + sizeof(size_t));
}

allocator_sorted_list::~allocator_sorted_list()
{
    if (_trusted_memory == nullptr) {
        return;
    }
    std::pmr::memory_resource* parent = get_parent(_trusted_memory);
    size_t space = get_space_size(_trusted_memory);
    size_t total_size = allocator_metadata_size + space;

    get_mutex(_trusted_memory).~mutex();

    if (parent == nullptr) {
        ::operator delete(_trusted_memory);
    }
    else {
        parent->deallocate(_trusted_memory, total_size);
    }
}

allocator_sorted_list::allocator_sorted_list(allocator_sorted_list &&other) noexcept
    : smart_mem_resource(std::move(other)), allocator_test_utils(std::move(other)), allocator_with_fit_mode(std::move(other))
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_sorted_list &allocator_sorted_list::operator=(allocator_sorted_list &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (_trusted_memory != nullptr) {
        std::pmr::memory_resource* old_parent = get_parent(_trusted_memory);
        size_t old_space = get_space_size(_trusted_memory);
        get_mutex(_trusted_memory).~mutex();
        if (old_parent == nullptr) {
            ::operator delete(_trusted_memory);
        }
        else {
            old_parent->deallocate(_trusted_memory, allocator_metadata_size + old_space);
        }
    }

    smart_mem_resource::operator=(std::move(other));
    allocator_test_utils::operator=(std::move(other));
    allocator_with_fit_mode::operator=(std::move(other));

    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    size_t total_size = allocator_metadata_size + space_size;
    if (parent_allocator == nullptr) {
        _trusted_memory = ::operator new(total_size);
    }
    else {
        _trusted_memory = parent_allocator->allocate(total_size);
    }

    try {
        new (&get_mutex(_trusted_memory)) std::mutex();
    } catch (...) {
        if (parent_allocator == nullptr) {
            ::operator delete(_trusted_memory);
        }
        else {
            parent_allocator->deallocate(_trusted_memory, total_size);
        }
        throw;
    }

    get_parent(_trusted_memory) = parent_allocator;
    get_space_size(_trusted_memory) = space_size;
    get_fit_mode(_trusted_memory) = allocate_fit_mode;

    if (space_size < block_metadata_size) {
        get_mutex(_trusted_memory).~mutex();
        if (parent_allocator == nullptr) {
            ::operator delete(_trusted_memory);
        }
        else {
            parent_allocator->deallocate(_trusted_memory, total_size);
        }
        throw std::invalid_argument("space_size too small");
    }

    void* first_block = static_cast<uint8_t*>(_trusted_memory) + allocator_metadata_size;
    get_first_free(_trusted_memory) = first_block;

    get_block_size(first_block) = space_size - block_metadata_size;
    get_next_free(first_block) = nullptr;
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(size_t size)
{
    if (size == 0) {
        throw std::invalid_argument("size must be > 0");
    }
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));

    allocator_with_fit_mode::fit_mode fit_mode = get_fit_mode(_trusted_memory);

    void* prev_free = nullptr;
    void* current_free = get_first_free(_trusted_memory);

    void* selected_prev = nullptr;
    void* selected_block = nullptr;

    while (current_free != nullptr) {
        size_t current_size = get_block_size(current_free);
        if (current_size >= size) {
            if (fit_mode == allocator_with_fit_mode::fit_mode::first_fit) {
                selected_block = current_free;
                selected_prev = prev_free;
                break;
            }
            else if (fit_mode == allocator_with_fit_mode::fit_mode::the_best_fit) {
                if (selected_block == nullptr || current_size < get_block_size(selected_block)) {
                    selected_block = current_free;
                    selected_prev = prev_free;
                }
            }
            else if (fit_mode == allocator_with_fit_mode::fit_mode::the_worst_fit) {
                if (selected_block == nullptr || current_size > get_block_size(selected_block)) {
                    selected_block = current_free;
                    selected_prev = prev_free;
                }
            }
        }
        prev_free = current_free;
        current_free = get_next_free(current_free);
    }

    if (selected_block == nullptr) {
        throw std::bad_alloc();
    }

    size_t selected_size = get_block_size(selected_block);
    void* selected_next = get_next_free(selected_block);

    if (selected_size >= size + block_metadata_size) {
        void* new_free_block = static_cast<uint8_t*>(selected_block) + block_metadata_size + size;
        get_block_size(new_free_block) = selected_size - size - block_metadata_size;
        get_next_free(new_free_block) = selected_next;

        if (selected_prev == nullptr) {
            get_first_free(_trusted_memory) = new_free_block;
        }
        else {
            get_next_free(selected_prev) = new_free_block;
        }

        get_block_size(selected_block) = size;
    } else {
        if (selected_prev == nullptr) {
            get_first_free(_trusted_memory) = selected_next;
        }
        else {
            get_next_free(selected_prev) = selected_next;
        }
    }

    return static_cast<uint8_t*>(selected_block) + block_metadata_size;
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
    : smart_mem_resource(other), allocator_test_utils(other), allocator_with_fit_mode(other)
{
    std::lock_guard<std::mutex> lock(get_mutex(other._trusted_memory));

    std::pmr::memory_resource* parent = get_parent(other._trusted_memory);
    size_t space = get_space_size(other._trusted_memory);
    size_t total_size = allocator_metadata_size + space;

    if (parent == nullptr) {
        _trusted_memory = ::operator new(total_size);
    }
    else {
        _trusted_memory = parent->allocate(total_size);
    }

    memcpy(_trusted_memory, other._trusted_memory, total_size);

    new (&get_mutex(_trusted_memory)) std::mutex();

    ptrdiff_t offset = static_cast<uint8_t*>(_trusted_memory) - static_cast<uint8_t*>(other._trusted_memory);

    if (get_first_free(_trusted_memory) != nullptr) {
        get_first_free(_trusted_memory) = static_cast<uint8_t*>(get_first_free(_trusted_memory)) + offset;
    }

    void* free_current = get_first_free(_trusted_memory);
    while (free_current != nullptr) {
        void* next_free = get_next_free(free_current);
        if (next_free != nullptr) {
            void* adjusted_next = static_cast<uint8_t*>(next_free) + offset;
            get_next_free(free_current) = adjusted_next;
            free_current = adjusted_next;
        }
        else {
            break;
        }
    }
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other)
{
    if (this == &other) {
        return *this;
    }
    std::lock_guard<std::mutex> lock(get_mutex(other._trusted_memory));
    std::pmr::memory_resource* parent = get_parent(other._trusted_memory);
    size_t space = get_space_size(other._trusted_memory);
    size_t total_size = allocator_metadata_size + space;

    void* new_memory = nullptr;
    if (parent == nullptr) {
        new_memory = ::operator new(total_size);
    }
    else {
        new_memory = parent->allocate(total_size);
    }

    if (_trusted_memory != nullptr) {
        std::pmr::memory_resource* old_parent = get_parent(_trusted_memory);
        size_t old_space = get_space_size(_trusted_memory);
        get_mutex(_trusted_memory).~mutex();
        if (old_parent == nullptr) {
            ::operator delete(_trusted_memory);
        }
        else {
            old_parent->deallocate(_trusted_memory, allocator_metadata_size + old_space);
        }
    }

    smart_mem_resource::operator=(other);
    allocator_test_utils::operator=(other);
    allocator_with_fit_mode::operator=(other);

    _trusted_memory = new_memory;
    memcpy(_trusted_memory, other._trusted_memory, total_size);

    new (&get_mutex(_trusted_memory)) std::mutex();

    ptrdiff_t offset = static_cast<uint8_t*>(_trusted_memory) - static_cast<uint8_t*>(other._trusted_memory);

    if (get_first_free(_trusted_memory) != nullptr) {
        get_first_free(_trusted_memory) = static_cast<uint8_t*>(get_first_free(_trusted_memory)) + offset;
    }

    void* free_current = get_first_free(_trusted_memory);
    while (free_current != nullptr) {
        void* next_free = get_next_free(free_current);
        if (next_free != nullptr) {
            void* adjusted_next = static_cast<uint8_t*>(next_free) + offset;
            get_next_free(free_current) = adjusted_next;
            free_current = adjusted_next;
        }
        else {
            break;
        }
    }

    return *this;
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    auto p = dynamic_cast<const allocator_sorted_list*>(&other);
    if (!p) {
        return false;
    }

    return this->_trusted_memory == p->_trusted_memory;
}

void allocator_sorted_list::do_deallocate_sm(void *at)
{
    if (at == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));

    void* block_to_free = static_cast<uint8_t*>(at) - block_metadata_size;

    void* current = get_first_free(_trusted_memory);
    void* prev = nullptr;

    while (current != nullptr && current < block_to_free) {
        prev = current;
        current = get_next_free(current);
    }

    get_next_free(block_to_free) = current;
    if (prev == nullptr) {
        get_first_free(_trusted_memory) = block_to_free;
    }
    else {
        get_next_free(prev) = block_to_free;
    }

    if (current != nullptr) {
        if (static_cast<uint8_t*>(block_to_free) + block_metadata_size + get_block_size(block_to_free) == current) {
            get_block_size(block_to_free) += block_metadata_size + get_block_size(current);
            get_next_free(block_to_free) = get_next_free(current);
        }
    }

    if (prev != nullptr) {
        if (static_cast<uint8_t*>(prev) + block_metadata_size + get_block_size(prev) == block_to_free) {
            get_block_size(prev) += block_metadata_size + get_block_size(block_to_free);
            get_next_free(prev) = get_next_free(block_to_free);
        }
    }
}

inline void allocator_sorted_list::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    if (_trusted_memory) {
        std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
        get_fit_mode(_trusted_memory) = mode;
    }
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    try {
        return get_blocks_info_inner();
    } catch (...) {
        return {};
    }
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> result;
    if (_trusted_memory == nullptr) {
        return result;
    }
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));

    void* current_block = static_cast<uint8_t*>(_trusted_memory) + allocator_metadata_size;
    void* end = static_cast<uint8_t*>(_trusted_memory) + allocator_metadata_size + get_space_size(_trusted_memory);

    void* free_block = get_first_free(_trusted_memory);

    while (current_block < end) {
        allocator_test_utils::block_info info;
        info.block_size = get_block_size(current_block);

        if (current_block == free_block) {
            info.is_block_occupied = false;
            free_block = get_next_free(free_block);
        }
        else {
            info.is_block_occupied = true;
        }

        result.push_back(info);

        current_block = static_cast<uint8_t*>(current_block) + block_metadata_size + info.block_size;
    }

    return result;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    return sorted_free_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator(nullptr);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator(nullptr);
}

bool allocator_sorted_list::sorted_free_iterator::operator==(const allocator_sorted_list::sorted_free_iterator & other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return _free_ptr != other._free_ptr;
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr) {
        _free_ptr = get_next_free(_free_ptr);
    }
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    sorted_free_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    if (_free_ptr) {
        return get_block_size(_free_ptr);
    }

    return 0;
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return _free_ptr;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator() : _free_ptr(nullptr)
{
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *trusted)
{
    if (trusted != nullptr) {
        _free_ptr = get_first_free(trusted);
    }
    else {
        _free_ptr = nullptr;
    }
}

bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator & other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return _current_ptr != other._current_ptr;
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr) {
        if (_current_ptr == _free_ptr) {
            _free_ptr = get_next_free(_free_ptr);
        }

        size_t b_size = get_block_size(_current_ptr);
        _current_ptr = static_cast<uint8_t*>(_current_ptr) + block_metadata_size + b_size;

        void* end = static_cast<uint8_t*>(_trusted_memory) + allocator_metadata_size + get_space_size(_trusted_memory);
        if (_current_ptr >= end) {
            _current_ptr = nullptr;
        }
    }

    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    sorted_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    if (_current_ptr) {
        return get_block_size(_current_ptr);
    }
    return 0;
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    return _current_ptr;
}

allocator_sorted_list::sorted_iterator::sorted_iterator() : _free_ptr(nullptr), _current_ptr(nullptr), _trusted_memory(nullptr)
{
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted) : _trusted_memory(trusted)
{
    if (trusted != nullptr) {
        size_t space = get_space_size(trusted);
        if (space > 0) {
            _current_ptr = static_cast<uint8_t*>(trusted) + allocator_metadata_size;
            _free_ptr = get_first_free(trusted);
        }
        else {
            _current_ptr = nullptr;
            _free_ptr = nullptr;
        }
    }
    else {
        _current_ptr = nullptr;
        _free_ptr = nullptr;
    }
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    return _current_ptr != _free_ptr;
}
