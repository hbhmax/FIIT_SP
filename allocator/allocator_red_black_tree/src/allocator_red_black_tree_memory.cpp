#include "allocator_red_black_tree_detail.h"
#include <cstddef>

namespace allocator_red_black_tree_detail
{
    char* bytes(void* p) noexcept
    {
        return static_cast<char*>(p);
    }

    const char* bytes(const void* p) noexcept
    {
        return static_cast<const char*>(p);
    }

    std::pmr::memory_resource*& parent_allocator_ref(void* trusted)
    {
        return *reinterpret_cast<std::pmr::memory_resource**>(trusted);
    }

    allocator_with_fit_mode::fit_mode& fit_mode_ref(void* trusted) noexcept
    {
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
            bytes(trusted) + sizeof(std::pmr::memory_resource*));
    }

    const allocator_with_fit_mode::fit_mode& fit_mode_ref(const void* trusted) noexcept
    {
        return *reinterpret_cast<const allocator_with_fit_mode::fit_mode*>(
            bytes(trusted) + sizeof(std::pmr::memory_resource*));
    }

    size_t& total_space_ref(void* trusted) noexcept
    {
        return *reinterpret_cast<size_t*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode));
    }

    const size_t& total_space_ref(const void* trusted) noexcept
    {
        return *reinterpret_cast<const size_t*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode));
    }

    std::mutex& mutex_ref(void* trusted) noexcept
    {
        return *reinterpret_cast<std::mutex*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t));
    }

    const std::mutex& mutex_ref(const void* trusted) noexcept
    {
        return *reinterpret_cast<const std::mutex*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t));
    }

    void*& root_ref(void* trusted)
    {
        return *reinterpret_cast<void**>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t)
            + sizeof(std::mutex));
    }

    void* const& root_ref(const void* trusted)
    {
        return *reinterpret_cast<void* const*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t)
            + sizeof(std::mutex));
    }

    block_data& block_data_ref(void* block)
    {
        return *reinterpret_cast<block_data*>(block);
    }

    block_data* block_data_ptr(void* block)
    {
        return reinterpret_cast<block_data*>(block);
    }

    bool is_occupied(void* block)
    {
        return block_data_ptr(block)->occupied;
    }

    block_color block_color_of(void* block)
    {
        return block == nullptr ? block_color::BLACK : block_data_ptr(block)->color;
    }

    void*& prev_block_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data));
    }

    void* const& prev_block_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data));
    }

    void*& next_block_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data) + sizeof(void*));
    }

    void* const& next_block_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data) + sizeof(void*));
    }

    void*& parent_block_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data) + 2 * sizeof(void*));
    }

    void* const& parent_block_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data) + 2 * sizeof(void*));
    }

    void*& left_child_block_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data) + 3 * sizeof(void*));
    }

    void* const& left_child_block_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data) + 3 * sizeof(void*));
    }

    void*& right_child_block_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data) + 4 * sizeof(void*));
    }

    void* const& right_child_block_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data) + 4 * sizeof(void*));
    }

    void* left_child_block_ptr(void* block) noexcept
    {
        if (block == nullptr) return nullptr;
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data) + 3 * sizeof(void*));
    }

    void* const left_child_block_ptr(const void* block) noexcept
    {
        if (block == nullptr) return nullptr;
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data) + 3 * sizeof(void*));
    }

    void* right_child_block_ptr(void* block) noexcept
    {
        if (block == nullptr) return nullptr;
        return *reinterpret_cast<void**>(bytes(block) + sizeof(block_data) + 4 * sizeof(void*));
    }

    void* const right_child_block_ptr(const void* block) noexcept
    {
        if (block == nullptr) return nullptr;
        return *reinterpret_cast<void* const*>(bytes(block) + sizeof(block_data) + 4 * sizeof(void*));
    }

    char* first_block_ptr(void* trusted) noexcept
    {
        return bytes(trusted) + allocator_metadata_size;
    }

    const char* first_block_ptr(const void* trusted) noexcept
    {
        return bytes(trusted) + allocator_metadata_size;
    }

    char* memory_end_ptr(void* trusted)
    {
        return first_block_ptr(trusted) + total_space_ref(trusted);
    }

    const char* memory_end_ptr(const void* trusted)
    {
        return first_block_ptr(trusted) + total_space_ref(trusted);
    }

    char* user_memory_ptr(void* block)
    {
        return bytes(block) + occupied_block_metadata_size;
    }

    const char* user_memory_ptr(const void* block)
    {
        return bytes(block) + occupied_block_metadata_size;
    }

    size_t block_size_of(void* block, void* trusted)
    {
        void* end = next_block_ref(block);
        if (end == nullptr)
            end = memory_end_ptr(trusted);

        return bytes(end) - bytes(block);
    }

    void reset_free_block_for_insert(void* block)
    {
        block_data_ref(block).occupied = false;
        block_data_ref(block).color = block_color::RED;
        left_child_block_ref(block) = nullptr;
        right_child_block_ref(block) = nullptr;
        parent_block_ref(block) = nullptr;
    }

    void reset_free_block(void* block)
    {
        reset_free_block_for_insert(block);
        prev_block_ref(block) = nullptr;
        next_block_ref(block) = nullptr;
    }

    void set_root_color(void* block)
    {
        block_data_ref(block).color = block_color::BLACK;
    }

    void release(void* trusted)
    {
        if (trusted == nullptr)
            return;

        auto* parent = parent_allocator_ref(trusted);
        size_t bytes_to_free = allocator_metadata_size + total_space_ref(trusted);

        mutex_ref(trusted).~mutex();
        parent->deallocate(trusted, bytes_to_free, alignof(std::max_align_t));
    }
}