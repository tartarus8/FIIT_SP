#include "../include/allocator_boundary_tags.h"
#include <stdexcept>
#include <cstring>
#include <vector>
#include <mutex>
#include <new>

static inline std::pmr::memory_resource*& get_parent(void* trusted) {
    return *reinterpret_cast<std::pmr::memory_resource**>(trusted);
}
static inline allocator_with_fit_mode::fit_mode& get_fit_mode(void* trusted) {
    return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::pmr::memory_resource*));
}
static inline size_t& get_space_size(void* trusted) {
    return *reinterpret_cast<size_t*>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode));
}
static inline std::mutex& get_mutex(void* trusted) {
    return *reinterpret_cast<std::mutex*>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t));
}
static inline void*& get_first_block(void* trusted) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(trusted) + sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t) + sizeof(std::mutex));
}

static inline size_t& get_block_payload_size(void* block) {
    return *reinterpret_cast<size_t*>(block);
}
static inline void*& get_block_prev(void* block) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(block) + sizeof(size_t));
}
static inline void*& get_block_next(void* block) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(block) + sizeof(size_t) + sizeof(void*));
}
static inline void*& get_block_allocator(void* block) {
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(block) + sizeof(size_t) + sizeof(void*) + sizeof(void*));
}


allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory == nullptr) {
        return;
    }
    std::pmr::memory_resource* parent = get_parent(_trusted_memory);
    size_t total_size = allocator_metadata_size + get_space_size(_trusted_memory);
    get_mutex(_trusted_memory).~mutex();
    if (parent == nullptr) {
        ::operator delete(_trusted_memory);
    }
    else {
        parent->deallocate(_trusted_memory, total_size);
    }
}

allocator_boundary_tags::allocator_boundary_tags(size_t space_size, std::pmr::memory_resource *parent_allocator, allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    if (parent_allocator == nullptr) {
        parent_allocator = std::pmr::get_default_resource();
    }

    size_t total_size = allocator_metadata_size + space_size;
    _trusted_memory = parent_allocator->allocate(total_size);

    get_parent(_trusted_memory) = parent_allocator;
    get_fit_mode(_trusted_memory) = allocate_fit_mode;
    get_space_size(_trusted_memory) = space_size;
    new (&get_mutex(_trusted_memory)) std::mutex();

    if (space_size < occupied_block_metadata_size) {
        throw std::invalid_argument("space too small");
    }
    void* first = reinterpret_cast<uint8_t*>(_trusted_memory) + allocator_metadata_size;
    get_first_block(_trusted_memory) = first;

    get_block_payload_size(first) = space_size - occupied_block_metadata_size;
    get_block_prev(first) = nullptr;
    get_block_next(first) = nullptr;
    get_block_allocator(first) = nullptr;
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(size_t size)
{
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));

    fit_mode mode = get_fit_mode(_trusted_memory);
    void* selected = nullptr;
    void* current = get_first_block(_trusted_memory);

    while (current) {
        if (get_block_allocator(current) == nullptr && get_block_payload_size(current) >= size) {
            if (mode == fit_mode::first_fit) {
                selected = current;
                break;
            }
            if (mode == fit_mode::the_best_fit && (!selected || get_block_payload_size(current) < get_block_payload_size(selected))) {
                selected = current;
            }
            if (mode == fit_mode::the_worst_fit && (!selected || get_block_payload_size(current) > get_block_payload_size(selected))) {
                selected = current;
            }
        }
        current = get_block_next(current);
    }

    if (!selected) {
        throw std::bad_alloc();
    }

    size_t total_available = get_block_payload_size(selected);
    if (total_available >= size + occupied_block_metadata_size) {
        void* new_block = reinterpret_cast<uint8_t*>(selected) + occupied_block_metadata_size + size;
        get_block_payload_size(new_block) = total_available - size - occupied_block_metadata_size;
        get_block_prev(new_block) = selected;
        get_block_next(new_block) = get_block_next(selected);
        get_block_allocator(new_block) = nullptr;

        if (get_block_next(selected)) {
            get_block_prev(get_block_next(selected)) = new_block;
        }
        get_block_next(selected) = new_block;
        get_block_payload_size(selected) = size;
    }

    get_block_allocator(selected) = _trusted_memory;

    return reinterpret_cast<uint8_t*>(selected) + occupied_block_metadata_size;
}

void allocator_boundary_tags::do_deallocate_sm(void *at)
{
    if (!at) {
        return;
    }
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    void* block = reinterpret_cast<uint8_t*>(at) - occupied_block_metadata_size;
    get_block_allocator(block) = nullptr;

    void* next = get_block_next(block);
    if (next && !get_block_allocator(next)) {
        get_block_payload_size(block) += occupied_block_metadata_size + get_block_payload_size(next);
        get_block_next(block) = get_block_next(next);
        if (get_block_next(block)) {
            get_block_prev(get_block_next(block)) = block;
        }
    }

    void* prev = get_block_prev(block);
    if (prev && !get_block_allocator(prev)) {
        get_block_payload_size(prev) += occupied_block_metadata_size + get_block_payload_size(block);
        get_block_next(prev) = get_block_next(block);
        if (get_block_next(block)) {
            get_block_prev(get_block_next(block)) = prev;
        }
    }
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    std::vector<allocator_test_utils::block_info> result;
    void* current = get_first_block(_trusted_memory);
    while (current) {
        result.push_back({get_block_payload_size(current) + occupied_block_metadata_size, get_block_allocator(current) != nullptr});
        current = get_block_next(current);
    }

    return result;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const {
    return get_blocks_info_inner();
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
    auto p = dynamic_cast<const allocator_boundary_tags*>(&other);

    return p && p->_trusted_memory == this->_trusted_memory;
}

inline void allocator_boundary_tags::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    get_fit_mode(_trusted_memory) = mode;
}

allocator_boundary_tags::allocator_boundary_tags(allocator_boundary_tags &&other) noexcept : _trusted_memory(other._trusted_memory) { other._trusted_memory = nullptr; }
allocator_boundary_tags &allocator_boundary_tags::operator=(allocator_boundary_tags &&other) noexcept {
    if (this != &other) {
        this->~allocator_boundary_tags();
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }

    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other) {
    std::lock_guard<std::mutex> lock(get_mutex(other._trusted_memory));
    size_t total = allocator_metadata_size + get_space_size(other._trusted_memory);
    std::pmr::memory_resource* parent = get_parent(other._trusted_memory);
    _trusted_memory = parent ? parent->allocate(total) : ::operator new(total);
    std::memcpy(_trusted_memory, other._trusted_memory, total);

    new (&get_mutex(_trusted_memory)) std::mutex();
    ptrdiff_t offset = reinterpret_cast<uint8_t*>(_trusted_memory) - reinterpret_cast<uint8_t*>(other._trusted_memory);
    get_first_block(_trusted_memory) = reinterpret_cast<uint8_t*>(get_first_block(_trusted_memory)) + offset;
    for (void* curr = get_first_block(_trusted_memory); curr; curr = get_block_next(curr)) {
        if (get_block_prev(curr)) {
            get_block_prev(curr) = reinterpret_cast<uint8_t*>(get_block_prev(curr)) + offset;
        }
        if (get_block_next(curr)) {
            get_block_next(curr) = reinterpret_cast<uint8_t*>(get_block_next(curr)) + offset;
        }
        if (get_block_allocator(curr)) {
            get_block_allocator(curr) = _trusted_memory;
        }
    }
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other) {
    if (this != &other) {
        allocator_boundary_tags tmp(other);
        std::swap(_trusted_memory, tmp._trusted_memory);
    }

    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    return boundary_iterator(_trusted_memory);
}
allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    return boundary_iterator();
}
allocator_boundary_tags::boundary_iterator::boundary_iterator() : _occupied_ptr(nullptr), _trusted_memory(nullptr)
{
}
allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted) : _trusted_memory(trusted)
{
    _occupied_ptr = trusted ? get_first_block(trusted) : nullptr;
}
bool allocator_boundary_tags::boundary_iterator::operator==(const boundary_iterator& other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr;
}
bool allocator_boundary_tags::boundary_iterator::operator!=(const boundary_iterator& other) const noexcept
{
    return !(*this == other);
}
allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (_occupied_ptr) {
        _occupied_ptr = get_block_next(_occupied_ptr);
        return *this;
    }
}
allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (_occupied_ptr) {
        _occupied_ptr = get_block_prev(_occupied_ptr);
        return *this;
    }
}
allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int)
{
    auto t = *this;
    ++(*this);
    return t;
}
allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int)
{
    auto t = *this;
    --(*this);
    return t;
}
size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    return _occupied_ptr ? get_block_payload_size(_occupied_ptr) + occupied_block_metadata_size : 0;
}
bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    return _occupied_ptr && get_block_allocator(_occupied_ptr);
}
void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    return _occupied_ptr;
}
void* allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept {
    return _occupied_ptr ? (uint8_t*)_occupied_ptr + occupied_block_metadata_size : nullptr;
}
