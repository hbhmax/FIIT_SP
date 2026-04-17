#include <not_implemented.h>

#include <cstring>
#include "../include/allocator_red_black_tree.h"
#include "allocator_red_black_tree_detail.h"

using namespace allocator_red_black_tree_detail;

allocator_red_black_tree::~allocator_red_black_tree()
{
    release(_trusted_memory);
}

allocator_red_black_tree::allocator_red_black_tree(
    allocator_red_black_tree &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(
    allocator_red_black_tree &&other) noexcept
{
    if (this == &other)
        return *this;

    release(_trusted_memory);
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_red_black_tree::allocator_red_black_tree(
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

    if (space_size >= free_block_metadata_size + 1)
    {
        void* first_block = first_block_ptr(_trusted_memory);
        root_ref(_trusted_memory) = first_block;
        reset_free_block(first_block);
        set_root_color(first_block);
    }
    else
    {
        root_ref(_trusted_memory) = nullptr;
    }
}

allocator_red_black_tree::allocator_red_black_tree(const allocator_red_black_tree &other)
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

    root_ref(_trusted_memory) = root_ref(other._trusted_memory) != nullptr
                                ? bytes(root_ref(other._trusted_memory)) - bytes(other._trusted_memory) + bytes(_trusted_memory)
                                : nullptr;

    void* old_cur = root_ref(other._trusted_memory) != nullptr
                    ? first_block_ptr(other._trusted_memory)
                    : nullptr;
    void* new_prev = nullptr;

    while (old_cur != nullptr)
    {
        ptrdiff_t offset = bytes(_trusted_memory) - bytes(other._trusted_memory);
        void* new_cur = bytes(old_cur) + offset;
        prev_block_ref(new_cur) = new_prev;
        if (new_prev != nullptr) next_block_ref(new_prev) = new_cur;
        if (is_occupied(old_cur))
        {
            parent_block_ref(new_cur) = _trusted_memory;
        }
        else
        {
            if (left_child_block_ref(old_cur) != nullptr)
                left_child_block_ref(new_cur) = bytes(left_child_block_ref(old_cur)) + offset;
            if (right_child_block_ref(old_cur) != nullptr)
                right_child_block_ref(new_cur) = bytes(right_child_block_ref(old_cur)) + offset;
            if (parent_block_ref(old_cur) != nullptr)
                parent_block_ref(new_cur) = bytes(parent_block_ref(old_cur)) + offset;
        }

        new_prev = new_cur;
        old_cur = next_block_ref(old_cur);
    }

    if (new_prev != nullptr)
        next_block_ref(new_prev) = nullptr;
}

allocator_red_black_tree &allocator_red_black_tree::operator=(const allocator_red_black_tree &other)
{
    if (this == &other)
        return *this;

    *this = allocator_red_black_tree(other);
    return *this;
}

bool allocator_red_black_tree::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == dynamic_cast<const allocator_red_black_tree*>(&other);
}

[[nodiscard]] void *allocator_red_black_tree::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr)
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));

    void* selected = nullptr;

    void* prev = nullptr;
    void* cur = root_ref(_trusted_memory);

    auto mode = fit_mode_ref(_trusted_memory);

    while (cur != nullptr)
    {
        size_t cur_size = block_size_of(cur, _trusted_memory);

        if (cur_size - occupied_block_metadata_size >= size)
        {
            if (mode == fit_mode::first_fit)
            {
                selected = cur;
                break;
            }
            else if (mode == fit_mode::the_best_fit 
                    && (selected == nullptr || cur_size < block_size_of(selected, _trusted_memory)))
            {
                selected = cur;
                cur = left_child_block_ref(cur);
            }
            else if (mode == fit_mode::the_worst_fit 
                    && (selected == nullptr || cur_size > block_size_of(selected, _trusted_memory)))
            {
                selected = cur;
                cur = right_child_block_ref(cur);
            }
        }
        else
        {
            cur = right_child_block_ref(cur);
        }
    }

    if (selected == nullptr)
        throw std::bad_alloc();

    size_t selected_size = block_size_of(selected, _trusted_memory);
    size_t selected_new_size = std::max(occupied_block_metadata_size + size, free_block_metadata_size + 1);
    size_t remainder = selected_size - selected_new_size;
    if (remainder >= free_block_metadata_size + 1)
    {
        void* new_free_block = bytes(selected) + selected_new_size;
        reset_free_block(new_free_block);

        prev_block_ref(new_free_block) = selected;
        next_block_ref(new_free_block) = next_block_ref(selected);
        block_data_ref(new_free_block).occupied = false;
        next_block_ref(selected) = new_free_block;
        if (next_block_ref(new_free_block) != nullptr)
            prev_block_ref(next_block_ref(new_free_block)) = new_free_block;

        // REMOVE OLD BLOCK
        remove_node(selected, _trusted_memory);
        // INSERT NEW BLOCK
        add_node(new_free_block, _trusted_memory);
    }
    else
    {
        // REMOVE OLD BLOCK
        remove_node(selected, _trusted_memory);
    }
    block_data_ref(selected).occupied = true;
    parent_block_ref(selected) = _trusted_memory;

    return user_memory_ptr(selected);
}

void allocator_red_black_tree::do_deallocate_sm(
    void *at)
{
    if (_trusted_memory == nullptr || at == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));

    char* p = bytes(at);
    char* begin_user_area = first_block_ptr(_trusted_memory) + occupied_block_metadata_size;
    char* end = memory_end_ptr(_trusted_memory);

    if (p < begin_user_area || p >= end)
        throw std::logic_error("pointer does not belong to this allocator");

    void* block = p - occupied_block_metadata_size;

    if (parent_block_ref(block) != _trusted_memory)
        throw std::logic_error("pointer does not belong to this allocator");

    if (!is_occupied(block))
        throw std::logic_error("block is already free");

    block_data_ref(block).occupied = false;
    void* prev_block = prev_block_ref(block);
    void* next_block = next_block_ref(block);
    if (prev_block != nullptr && !is_occupied(prev_block))
    {
        // REMOVE PREV BLOCK
        remove_node(prev_block, _trusted_memory);

        next_block_ref(prev_block) = next_block;
        block = prev_block;
        if (next_block != nullptr)
            prev_block_ref(next_block) = block;

    }

    if (next_block != nullptr && !is_occupied(next_block))
    {
        // REMOVE NEXT BLOCK
        remove_node(next_block, _trusted_memory);
        next_block = next_block_ref(next_block);
        next_block_ref(block) = next_block;
        if (next_block != nullptr)
            prev_block_ref(next_block) = block;
    }

    reset_free_block_for_insert(block);
    // ADD NEW CUR BLOCK
    add_node(block, _trusted_memory);
}

void allocator_red_black_tree::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    fit_mode_ref(_trusted_memory) = mode;
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info() const
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_red_black_tree::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> vec;

    for (auto it = begin(); it != end(); it++)
    {
        vec.push_back({ it.size(), it.occupied() });
    }

    return vec;
}


allocator_red_black_tree::rb_iterator allocator_red_black_tree::begin() const noexcept
{
    return rb_iterator(_trusted_memory);
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::end() const noexcept
{
    return rb_iterator();
}


bool allocator_red_black_tree::rb_iterator::operator==(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return _block_ptr == other._block_ptr;
}

bool allocator_red_black_tree::rb_iterator::operator!=(const allocator_red_black_tree::rb_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_red_black_tree::rb_iterator &allocator_red_black_tree::rb_iterator::operator++() & noexcept
{
    if (_block_ptr != nullptr)
        _block_ptr = next_block_ref(_block_ptr);
    return *this;
}

allocator_red_black_tree::rb_iterator allocator_red_black_tree::rb_iterator::operator++(int n)
{
    auto copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_red_black_tree::rb_iterator::size() const noexcept
{
    if (_block_ptr == nullptr) return 0;

    size_t size = block_size_of(_block_ptr, _trusted);
    if (is_occupied(_block_ptr)) return size - occupied_block_metadata_size;

    return size - free_block_metadata_size;
}

void *allocator_red_black_tree::rb_iterator::operator*() const noexcept
{
    return _block_ptr;
}

allocator_red_black_tree::rb_iterator::rb_iterator()
    : _trusted(nullptr), _block_ptr(nullptr)
{
}

allocator_red_black_tree::rb_iterator::rb_iterator(void *trusted)
    : _trusted(nullptr), _block_ptr(nullptr)
{
    if (trusted == nullptr)
        return;

    _trusted = trusted;
    if (root_ref(trusted) != nullptr)
        _block_ptr = first_block_ptr(trusted);
}

bool allocator_red_black_tree::rb_iterator::occupied() const noexcept
{
    if (_block_ptr == nullptr) return false;

    return is_occupied(_block_ptr);
}
