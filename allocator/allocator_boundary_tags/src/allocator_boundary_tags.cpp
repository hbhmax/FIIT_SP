#include <not_implemented.h>
#include <cstring>
#include "../include/allocator_boundary_tags.h"

namespace
{
    inline char* bytes(void* p) noexcept
    {
        return static_cast<char*>(p);
    }

    inline const char* bytes(const void* p) noexcept
    {
        return static_cast<const char*>(p);
    }

    inline std::pmr::memory_resource*& parent_allocator_ref(void* trusted) {
        return *reinterpret_cast<std::pmr::memory_resource**>(trusted);
    }

    inline allocator_with_fit_mode::fit_mode& fit_mode_ref(void* trusted) noexcept
    {
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
            bytes(trusted) + sizeof(std::pmr::memory_resource*));
    }

    inline const allocator_with_fit_mode::fit_mode& fit_mode_ref(const void* trusted) noexcept
    {
        return *reinterpret_cast<const allocator_with_fit_mode::fit_mode*>(
            bytes(trusted) + sizeof(std::pmr::memory_resource*));
    }

    inline size_t& total_space_ref(void* trusted) noexcept
    {
        return *reinterpret_cast<size_t*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode));
    }

    inline const size_t& total_space_ref(const void* trusted) noexcept
    {
        return *reinterpret_cast<const size_t*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode));
    }

    inline std::mutex& mutex_ref(void* trusted) noexcept
    {
        return *reinterpret_cast<std::mutex*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t));
    }

    inline const std::mutex& mutex_ref(const void* trusted) noexcept
    {
        return *reinterpret_cast<const std::mutex*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t));
    }

    inline void*& first_occupied_ref(void* trusted) {
        return *reinterpret_cast<void**>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t)
            + sizeof(std::mutex));
    }

    inline void* const& first_occupied_ref(const void* trusted) {
        return *reinterpret_cast<void* const*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t)
            + sizeof(std::mutex));
    }

    inline size_t& block_size_ref(void* block) noexcept
    {
        return *reinterpret_cast<size_t*>(block);
    }

    inline const size_t& block_size_ref(const void* block) noexcept
    {
        return *reinterpret_cast<const size_t*>(block);
    }

    inline void*& prev_occupied_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + sizeof(size_t));
    }

    inline void* const& prev_occupied_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(size_t));
    }

    inline void*& next_occupied_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(
            bytes(block)
            + sizeof(size_t)
            + sizeof(void*));
    }

    inline void* const& next_occupied_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(
            bytes(block)
            + sizeof(size_t)
            + sizeof(void*));
    }

    inline void*& memory_begin_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(
            bytes(block)
            + sizeof(size_t)
            + sizeof(void*)
            + sizeof(void*));
    }

    inline void* const& memory_begin_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(
            bytes(block)
            + sizeof(size_t)
            + sizeof(void*)
            + sizeof(void*));
    }

    constexpr const size_t allocator_metadata_size =
        sizeof(std::pmr::memory_resource*)
        + sizeof(allocator_with_fit_mode::fit_mode)
        + sizeof(size_t)
        + sizeof(std::mutex)
        + sizeof(void*);

    constexpr const size_t block_metadata_size = sizeof(size_t) + sizeof(void*) + sizeof(void*) + sizeof(void*);

    inline char* first_block_ptr(void* trusted) noexcept
    {
        return bytes(trusted) + allocator_metadata_size;
    }

    inline const char* first_block_ptr(const void* trusted) noexcept
    {
        return bytes(trusted) + allocator_metadata_size;
    }

    inline char* memory_end_ptr(void* trusted)
    {
        return first_block_ptr(trusted) + total_space_ref(trusted);
    }

    inline const char* memory_end_ptr(const void* trusted)
    {
        return first_block_ptr(trusted) + total_space_ref(trusted);
    }

    inline char* user_memory_ptr(void* block) {
        return bytes(block) + block_metadata_size;
    }

    inline const char* user_memory_ptr(const void* block) {
        return bytes(block) + block_metadata_size;
    }

    inline char* next_physical_block(void* block) {
        return bytes(block)
            + block_metadata_size
            + block_size_ref(block);
    }

    inline const char* next_physical_block(const void* block) {
        return bytes(block)
            + block_metadata_size
            + block_size_ref(block);
    }
}

allocator_boundary_tags::~allocator_boundary_tags()
{
    if (_trusted_memory == nullptr) return;
    auto* parent = parent_allocator_ref(_trusted_memory);
    size_t bytes_to_free = allocator_metadata_size + total_space_ref(_trusted_memory);

    mutex_ref(_trusted_memory).~mutex();
    parent->deallocate(_trusted_memory, bytes_to_free, alignof(std::max_align_t));

    _trusted_memory = nullptr;
}

allocator_boundary_tags::allocator_boundary_tags(
    allocator_boundary_tags &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(
    allocator_boundary_tags &&other) noexcept
{
    if (this == &other)
        return *this;

    this->~allocator_boundary_tags();
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    auto* parent = parent_allocator == nullptr
        ? std::pmr::get_default_resource()
        : parent_allocator;

    _trusted_memory = parent->allocate(
        allocator_metadata_size + space_size,
        alignof(std::max_align_t));

    parent_allocator_ref(_trusted_memory) = parent;
    fit_mode_ref(_trusted_memory) = allocate_fit_mode;
    total_space_ref(_trusted_memory) = space_size;
    new(&mutex_ref(_trusted_memory)) std::mutex();
    first_occupied_ref(_trusted_memory) = nullptr;
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr)
        return nullptr;
    
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));

    void* selected_prev_occupied = nullptr;
    void* selected_next_occupied = nullptr;
    void* selected_start = nullptr;


    void* prev_occupied_block = nullptr;
    void* free_block_start = first_block_ptr(_trusted_memory);
    void* next_occupied_block = first_occupied_ref(_trusted_memory);

    auto mode = fit_mode_ref(_trusted_memory);


    while (free_block_start != nullptr)
    {
        ptrdiff_t cur_size = (next_occupied_block != nullptr
            ? bytes(next_occupied_block)
            : memory_end_ptr(_trusted_memory)) - bytes(free_block_start);

        if (cur_size >= block_metadata_size + size)
        {
            ptrdiff_t selected_size = 0;
            if (selected_start != nullptr)
            {
                selected_size = (selected_next_occupied != nullptr
                    ? bytes(selected_next_occupied)
                    : memory_end_ptr(_trusted_memory)) - bytes(selected_start);

            }
            if (selected_start == nullptr
                || (mode == fit_mode::first_fit)
                || (mode == fit_mode::the_best_fit
                    && cur_size < selected_size)
                || (mode == fit_mode::the_worst_fit
                    && cur_size > selected_size))
            {
                selected_prev_occupied = prev_occupied_block;
                selected_next_occupied = next_occupied_block;
                selected_start = free_block_start;
                if (mode == fit_mode::first_fit) break;
            }
        }

        if (next_occupied_block == nullptr) break;

        free_block_start = next_physical_block(next_occupied_block);
        prev_occupied_block = next_occupied_block;
        next_occupied_block = next_occupied_ref(next_occupied_block);
    }

    if (selected_start == nullptr)
        throw std::bad_alloc();

    ptrdiff_t selected_size = 
        (selected_next_occupied != nullptr
            ? bytes(selected_next_occupied)
            : memory_end_ptr(_trusted_memory))
        - bytes(selected_start);

    size_t remainder = selected_size - (size + block_metadata_size);
    if (remainder >= block_metadata_size + 1)
        block_size_ref(selected_start) = size;
    else
        block_size_ref(selected_start) = selected_size - block_metadata_size;

    prev_occupied_ref(selected_start) = selected_prev_occupied;
    next_occupied_ref(selected_start) = selected_next_occupied;
    memory_begin_ref(selected_start) = _trusted_memory;
    
    if (selected_prev_occupied == nullptr)
        first_occupied_ref(_trusted_memory) = selected_start;
    else
        next_occupied_ref(selected_prev_occupied) = selected_start;

    if (selected_next_occupied != nullptr)
        prev_occupied_ref(selected_next_occupied) = selected_start;

    return user_memory_ptr(selected_start);
}

void allocator_boundary_tags::do_deallocate_sm(void *at)
{
    if (_trusted_memory == nullptr || at == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));

    char* p = bytes(at);
    char* begin_user_area = first_block_ptr(_trusted_memory) + block_metadata_size;
    char* end = memory_end_ptr(_trusted_memory);

    if (p < begin_user_area || p >= end)
        throw std::logic_error("pointer does not belong to this allocator");

    void* block = p - block_metadata_size;

    if (memory_begin_ref(block) != _trusted_memory)
        throw std::logic_error("pointer does not belong to this allocator or block is already free");

    void* prev_block = prev_occupied_ref(block);
    void* next_block = next_occupied_ref(block);
    if (prev_block != nullptr)
        next_occupied_ref(prev_block) = next_block;
    else
        first_occupied_ref(_trusted_memory) = next_block;
    if (next_block != nullptr)
        prev_occupied_ref(next_block) = prev_block;

    memory_begin_ref(block) = nullptr;
    prev_occupied_ref(block) = nullptr;
    next_occupied_ref(block) = nullptr;
}

inline void allocator_boundary_tags::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    fit_mode_ref(_trusted_memory) = mode;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    return get_blocks_info_inner();
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept
{
    return boundary_iterator(_trusted_memory);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept
{
    return boundary_iterator();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> vec;

    for (auto it = begin(); it != end(); it++)
    {
        vec.push_back({ it.size(), it.occupied() });
    }

    return vec;
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags& other)
    : _trusted_memory(nullptr)
{
    if (other._trusted_memory == nullptr)
        return;

    auto* parent = parent_allocator_ref(other._trusted_memory);
    size_t space = total_space_ref(other._trusted_memory);

    _trusted_memory = parent->allocate(
        allocator_metadata_size + space,
        alignof(std::max_align_t));

    parent_allocator_ref(_trusted_memory) = parent;
    fit_mode_ref(_trusted_memory) = fit_mode_ref(other._trusted_memory);
    total_space_ref(_trusted_memory) = space;
    new(&mutex_ref(_trusted_memory)) std::mutex();

    std::memcpy(
        first_block_ptr(_trusted_memory),
        first_block_ptr(other._trusted_memory),
        space);

    first_occupied_ref(_trusted_memory) = nullptr;

    void* old_cur = first_occupied_ref(other._trusted_memory);
    void* new_prev = nullptr;

    while (old_cur != nullptr)
    {
        ptrdiff_t offset = bytes(old_cur) - bytes(other._trusted_memory);
        void* new_cur = bytes(_trusted_memory) + offset;
        memory_begin_ref(new_cur) = _trusted_memory;

        if (new_prev == nullptr)
        {
            first_occupied_ref(_trusted_memory) = new_cur;
            prev_occupied_ref(new_cur) = nullptr;
        }
        else
        {
            next_occupied_ref(new_prev) = new_cur;
            prev_occupied_ref(new_cur) = new_prev;
        }

        new_prev = new_cur;
        old_cur = next_occupied_ref(old_cur);
    }

    if (new_prev != nullptr)
        next_occupied_ref(new_prev) = nullptr;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other)
{
    if (this == &other)
        return *this;

    *this = allocator_boundary_tags(other);
    return *this;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == dynamic_cast<const allocator_boundary_tags*>(&other);
}

bool allocator_boundary_tags::boundary_iterator::operator==(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    return _occupied_ptr == other._occupied_ptr && _occupied == other._occupied;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(
        const allocator_boundary_tags::boundary_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator++() & noexcept
{
    if (_occupied_ptr == nullptr && occupied())
        return *this;

    if (_occupied_ptr == nullptr)
    {
        _occupied_ptr = first_occupied_ref(_trusted_memory);
        _occupied = true;
        return *this;
    }

    if (occupied())
    {
        void* new_ptr = next_occupied_ref(_occupied_ptr);
        if (new_ptr != nullptr && bytes(new_ptr) == next_physical_block(_occupied_ptr))
            _occupied_ptr = new_ptr;
        else
            _occupied = false;

    }
    else
    {
        _occupied_ptr = next_occupied_ref(_occupied_ptr);
        _occupied = true;
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator &allocator_boundary_tags::boundary_iterator::operator--() & noexcept
{
    if (_occupied_ptr == nullptr)
        return *this;

    if (occupied())
    {
        void* prev_ptr = prev_occupied_ref(_occupied_ptr);
        if ((prev_ptr != nullptr && next_physical_block(prev_ptr) != bytes(_occupied_ptr))
                || (prev_ptr == nullptr && first_block_ptr(_trusted_memory) != _occupied_ptr))
            _occupied = false;

        _occupied_ptr = prev_ptr;
    }
    else
        _occupied = true;
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n)
{
    auto copy = *this;
    ++(*this);
    return copy;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n)
{
    auto copy = *this;
    --(*this);
    return copy;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept
{
    if (_occupied_ptr == nullptr)
    {
        if (occupied()) return 0;

        void* first_occ = first_occupied_ref(_trusted_memory);
        if (first_occ == nullptr) return memory_end_ptr(_trusted_memory) - first_block_ptr(_trusted_memory);

        return bytes(first_occ) - first_block_ptr(_trusted_memory);
    }

    if (occupied())
        return occupied_block_metadata_size + block_size_ref(_occupied_ptr);


    void* next_occ = next_occupied_ref(_occupied_ptr);
    if (next_occ == nullptr)
        return memory_end_ptr(_trusted_memory) - next_physical_block(_occupied_ptr);

    return bytes(next_occ) - next_physical_block(_occupied_ptr);
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept
{
    return _occupied;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept
{
    if (_occupied_ptr == nullptr)
    {
        if (occupied())
            return nullptr;

        return first_block_ptr(_trusted_memory);
    }

    if (occupied()) return _occupied_ptr;

    return next_physical_block(_occupied_ptr);
}

allocator_boundary_tags::boundary_iterator::boundary_iterator()

    : _occupied_ptr(nullptr), _occupied(true), _trusted_memory(nullptr)
{
}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void *trusted)
    : _occupied_ptr(nullptr), _occupied(true), _trusted_memory(trusted)
{
    if (trusted == nullptr)
        return;

    
    void* first_occ = first_occupied_ref(trusted);
    if (first_occ != nullptr && first_block_ptr(trusted) == bytes(first_occ))
    {
        _occupied_ptr = first_occ;
        _occupied = true;
    }
    else
    {
        _occupied_ptr = nullptr;
        _occupied = false;
    }
}

void *allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept
{
    return _occupied_ptr;
}
