#include <not_implemented.h>
#include <cstddef>
#include <cstring>
#include "../include/allocator_buddies_system.h"

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

    constexpr size_t align_up(size_t n, size_t alignment)
    {
        return (n + alignment - 1) / alignment * alignment;
    }

    constexpr size_t floor_k_of_2(size_t n)
    {
        size_t k = 0;
        while ((size_t{ 1 } << (k + 1)) <= n) k++;
        return k;
    }

    constexpr size_t allocator_header_size()
    {
        size_t off = sizeof(std::pmr::memory_resource*);

        off = align_up(off, alignof(allocator_with_fit_mode::fit_mode));
        off += sizeof(allocator_with_fit_mode::fit_mode);

        off = align_up(off, alignof(unsigned char));
        off += sizeof(unsigned char);

        off = align_up(off, alignof(std::mutex));
        off += sizeof(std::mutex);

        off = align_up(off, alignof(std::max_align_t));

        return off;
    }

    struct block_metadata
    {
        bool occupied : 1;
        unsigned char size : 7;
    };

    constexpr inline size_t owner_offset()
    {
        return align_up(sizeof(block_metadata), alignof(void*));
    }

    constexpr size_t occ_block_md_size()
    {
        size_t occupied_block_max_size = align_up(owner_offset() + sizeof(void*), alignof(std::max_align_t));

        return occupied_block_max_size;
    }

    inline std::pmr::memory_resource*& parent_allocator_ref(void* trusted) {
        return *reinterpret_cast<std::pmr::memory_resource**>(trusted);
    }

    inline allocator_with_fit_mode::fit_mode& fit_mode_ref(void* trusted)
    {
        size_t off = sizeof(std::pmr::memory_resource*);
        off = align_up(off, alignof(allocator_with_fit_mode::fit_mode));
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(bytes(trusted) + off);
    }

    inline unsigned char& total_space_k_ref(void* trusted)
    {
        size_t off = sizeof(std::pmr::memory_resource*);

        off = align_up(off, alignof(allocator_with_fit_mode::fit_mode));
        off += sizeof(allocator_with_fit_mode::fit_mode);

        off = align_up(off, alignof(unsigned char));
        return *reinterpret_cast<unsigned char*>(bytes(trusted) + off);
    }

    inline size_t total_space(void* trusted)
    {
        return size_t{ 1 } << total_space_k_ref(trusted);
    }

    inline std::mutex& mutex_ref(void* trusted)
    {
        size_t off = sizeof(std::pmr::memory_resource*);

        off = align_up(off, alignof(allocator_with_fit_mode::fit_mode));
        off += sizeof(allocator_with_fit_mode::fit_mode);

        off = align_up(off, alignof(unsigned char));
        off += sizeof(unsigned char);

        off = align_up(off, alignof(std::mutex));
        return *reinterpret_cast<std::mutex *>(bytes(trusted) + off);
    }

    inline char* first_block_ptr(void* trusted) noexcept
    {
        return bytes(trusted) + allocator_header_size();
    }

    inline block_metadata* first_block_md_ptr(void* trusted) noexcept
    {
        return reinterpret_cast<block_metadata*>(first_block_ptr(trusted));
    }

    inline char* memory_end_ptr(void* trusted)
    {
        return first_block_ptr(trusted) + total_space(trusted);
    }

    inline block_metadata& block_md_ref(void* block)
    {
        return *reinterpret_cast<block_metadata*>(block);
    }

    inline block_metadata* block_md_ptr(void* block) noexcept
    {
        return reinterpret_cast<block_metadata*>(block);
    }


    inline size_t block_size_of(void* block)
    {
        return size_t{ 1 } << block_md_ref(block).size;
    }

    inline char* next_physical_block(void* block) {
        return bytes(block)
            + block_size_of(block);
    }

    inline void*& owner_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + owner_offset());
    }

    inline void release(void* trusted)
    {
        if (trusted == nullptr)
            return;

        auto* parent = parent_allocator_ref(trusted);
        size_t bytes_to_free = allocator_header_size() + total_space(trusted);

        mutex_ref(trusted).~mutex();
        parent->deallocate(trusted, bytes_to_free, alignof(std::max_align_t));
        trusted = nullptr;
    }

}

allocator_buddies_system::~allocator_buddies_system()
{
    release(_trusted_memory);
}

allocator_buddies_system::allocator_buddies_system(
    allocator_buddies_system &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_buddies_system &allocator_buddies_system::operator=(
    allocator_buddies_system &&other) noexcept
{
    if (this == &other)
        return *this;

    release(_trusted_memory);
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_buddies_system::allocator_buddies_system(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{
    constexpr size_t min_k = __detail::nearest_greater_k_of_2(occ_block_md_size());

    if (space_size < (size_t{ 1 }) << min_k)
        throw std::logic_error("space size is too small");

    size_t total_space_k = __detail::nearest_greater_k_of_2(space_size);
    size_t bytes = size_t{ 1 } << total_space_k;

    auto* parent = parent_allocator == nullptr
        ? std::pmr::get_default_resource()
        : parent_allocator;

    _trusted_memory = parent->allocate(
        allocator_header_size() + bytes,
        alignof(std::max_align_t));

    parent_allocator_ref(_trusted_memory) = parent;
    fit_mode_ref(_trusted_memory) = allocate_fit_mode;
    total_space_k_ref(_trusted_memory) = static_cast<unsigned char>(total_space_k);
    new(&mutex_ref(_trusted_memory)) std::mutex();

    auto* first_block = first_block_md_ptr(_trusted_memory);

    first_block->occupied = false;
    first_block->size = static_cast<unsigned char>(total_space_k);
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr)
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));

    size_t req_total = occ_block_md_size() + size;
    size_t req_k = __detail::nearest_greater_k_of_2(req_total);
    size_t total_space_k = total_space_k_ref(_trusted_memory);

    if (req_k > total_space_k)
        throw std::bad_alloc();

    void* selected = nullptr;
    size_t selected_k = 0;
    auto mode = fit_mode_ref(_trusted_memory);

    for (auto it = begin(), it_end = end(); it != it_end; it++)
    {
        if (it.occupied())
            continue;

        size_t cur_k = block_md_ref(*it).size;

        if (cur_k < req_k)
            continue;

        if (mode == fit_mode::first_fit)
        {
            selected = *it;
            selected_k = cur_k;
            break;
        }

        if (selected == nullptr
            || (mode == fit_mode::the_best_fit && cur_k < selected_k)
            || (mode == fit_mode::the_worst_fit && cur_k > selected_k))
        {
            selected = *it;
            selected_k = cur_k;
        }
    }

    if (selected == nullptr)
        throw std::bad_alloc();

    auto* res_block_md = block_md_ptr(selected);

    while (selected_k > req_k)
    {
        size_t new_k = selected_k - 1;
        size_t new_bytes = size_t{ 1 } << new_k;

        auto* left = block_md_ptr(selected);
        auto* right = block_md_ptr(bytes(selected) + new_bytes);

        left->occupied = false;
        left->size = static_cast<unsigned char>(new_k);

        right->occupied = false;
        right->size = static_cast<unsigned char>(new_k);

        res_block_md = left;
        selected_k = new_k;
    }

    res_block_md->occupied = true;
    res_block_md->size = static_cast<unsigned char>(selected_k);
    owner_ref(selected) = _trusted_memory;

    return bytes(selected) + occ_block_md_size();
}

void allocator_buddies_system::do_deallocate_sm(void *at)
{
    if (_trusted_memory == nullptr || at == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));


    char* p = bytes(at);
    char* begin_user_area = first_block_ptr(_trusted_memory);
    char* end = begin_user_area + (size_t{ 1 } << total_space_k_ref(_trusted_memory));

    if (p < begin_user_area || p >= end)
        throw std::logic_error("pointer does not belong to this allocator");

    char* block = p - occ_block_md_size();
    auto* md = block_md_ptr(block);

    if (!md->occupied)
        throw std::logic_error("block is already free");

    if (owner_ref(block) != _trusted_memory)
        throw std::logic_error("pointer does not belong to this allocator");

    md->occupied = false;

    size_t total_space_k = total_space_k_ref(_trusted_memory);

    while (md->size < total_space_k)
    {
        size_t cur_size = size_t{ 1 } << md->size;
        size_t offset = static_cast<size_t>(block - begin_user_area);
        char* buddy = begin_user_area + (offset ^ cur_size);

        if (buddy < begin_user_area || buddy >= end)
            break;

        auto* buddy_md = block_md_ptr(buddy);

        if (buddy_md->occupied || buddy_md->size != md->size)
            break;

        block = (buddy < block ? buddy : block);
        md = block_md_ptr(block);
        md->occupied = false;
        md->size = static_cast<unsigned char>(md->size + 1);
    }
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system& other)
    : _trusted_memory(nullptr)
{
    if (other._trusted_memory == nullptr)
        return;

    auto* parent = parent_allocator_ref(other._trusted_memory);
    size_t space = total_space(other._trusted_memory);
    
    _trusted_memory = parent->allocate(
        allocator_header_size() + space,
        alignof(std::max_align_t));

    parent_allocator_ref(_trusted_memory) = parent;
    fit_mode_ref(_trusted_memory) = fit_mode_ref(other._trusted_memory);
    total_space_k_ref(_trusted_memory) = total_space_k_ref(other._trusted_memory);
    new(&mutex_ref(_trusted_memory)) std::mutex();

    std::memcpy(
        first_block_ptr(_trusted_memory),
        first_block_ptr(other._trusted_memory),
        space);

    for (auto it = begin(), it_end = end(); it != it_end; it++)
    {
        if (it.occupied())
            owner_ref(*it) = _trusted_memory;
    }
}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other)
{
    if (this == &other)
        return *this;

    *this = allocator_buddies_system(other);
    return *this;
}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == dynamic_cast<const allocator_buddies_system*>(&other);
}

inline void allocator_buddies_system::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    fit_mode_ref(_trusted_memory) = mode;
}


std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> vec;

    for (auto it = begin(); it != end(); it++)
    {
        vec.push_back({ it.size(), it.occupied() });
    }

    return vec;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept
{
    return buddy_iterator(first_block_ptr(_trusted_memory));
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept
{
    return buddy_iterator(memory_end_ptr(_trusted_memory));
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept
{
    if (_block != nullptr)
    {
        _block = bytes(_block) + size();
    }
    return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int n)
{
    auto copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept
{
    if (_block == nullptr)
        return 0;
    return block_size_of(_block);
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept
{
    if (_block == nullptr)
        return false;

    return block_md_ref(_block).occupied;
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept
{
    return _block;
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start)
    : _block(start)
{
}

allocator_buddies_system::buddy_iterator::buddy_iterator()
    : _block(nullptr)
{
}