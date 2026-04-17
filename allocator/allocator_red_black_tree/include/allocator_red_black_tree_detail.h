#pragma once

#include <memory_resource>
#include <mutex>
#include "allocator_with_fit_mode.h"

namespace allocator_red_black_tree_detail
{
    char* bytes(void* p) noexcept;
    const char* bytes(const void* p) noexcept;

    enum class block_color : unsigned char
    {
        RED, BLACK
    };

    struct block_data
    {
        bool occupied : 4;
        block_color color : 4;
    };

    inline constexpr size_t allocator_metadata_size =
        sizeof(std::pmr::memory_resource*) +
        sizeof(allocator_with_fit_mode::fit_mode) +
        sizeof(size_t) +
        sizeof(std::mutex) +
        sizeof(void*);

    inline constexpr size_t occupied_block_metadata_size =
        sizeof(block_data) + 3 * sizeof(void*);

    inline constexpr size_t free_block_metadata_size =
        sizeof(block_data) + 5 * sizeof(void*);

    std::pmr::memory_resource*& parent_allocator_ref(void* trusted);
    allocator_with_fit_mode::fit_mode& fit_mode_ref(void* trusted) noexcept;
    const allocator_with_fit_mode::fit_mode& fit_mode_ref(const void* trusted) noexcept;
    size_t& total_space_ref(void* trusted) noexcept;
    const size_t& total_space_ref(const void* trusted) noexcept;
    std::mutex& mutex_ref(void* trusted) noexcept;
    const std::mutex& mutex_ref(const void* trusted) noexcept;
    void*& root_ref(void* trusted);
    void* const& root_ref(const void* trusted);

    block_data& block_data_ref(void* block);
    block_data* block_data_ptr(void* block);
    bool is_occupied(void* block);
    block_color block_color_of(void* block);

    void*& prev_block_ref(void* block) noexcept;
    void* const& prev_block_ref(const void* block) noexcept;

    void*& next_block_ref(void* block) noexcept;
    void* const& next_block_ref(const void* block) noexcept;

    void*& parent_block_ref(void* block) noexcept;
    void* const& parent_block_ref(const void* block) noexcept;

    void*& left_child_block_ref(void* block) noexcept;
    void* const& left_child_block_ref(const void* block) noexcept;

    void*& right_child_block_ref(void* block) noexcept;
    void* const& right_child_block_ref(const void* block) noexcept;

    void* left_child_block_ptr(void* block) noexcept;
    void* const left_child_block_ptr(const void* block) noexcept;

    void* right_child_block_ptr(void* block) noexcept;
    void* const right_child_block_ptr(const void* block) noexcept;

    char* first_block_ptr(void* trusted) noexcept;
    const char* first_block_ptr(const void* trusted) noexcept;

    char* memory_end_ptr(void* trusted);
    const char* memory_end_ptr(const void* trusted);

    char* user_memory_ptr(void* block);
    const char* user_memory_ptr(const void* block);

    size_t block_size_of(void* block, void* trusted);

    void reset_free_block_for_insert(void* block);
    void reset_free_block(void* block);
    void set_root_color(void* block);
    void release(void* trusted);

    void transplant(void* u, void* v, void* trusted);
    void rotate_left(void* x, void* trusted);
    void rotate_right(void* x, void* trusted);

    void* grandparent(void* node);
    void* uncle(void* node);
    void* sibling(void* parent, void* child);

    bool is_right_child(void* node);
    bool is_left_child(void* node);

    void insert_case_1(void* node, void* trusted);
    void insert_case_2(void* node, void* trusted);
    void insert_case_3(void* node, void* trusted);
    void insert_case_4(void* node, void* trusted);
    void insert_case_5(void* node, void* trusted);

    void on_node_added(void* block, void* trusted);
    void add_node(void* block, void* trusted);

    void remove_case_1(void* parent, void* child, void* trusted);
    void remove_case_2(void* parent, void* child, void* trusted);
    void remove_case_3(void* parent, void* child, void* trusted);
    void remove_case_4(void* parent, void* child, void* trusted);
    void remove_case_5(void* parent, void* child, void* trusted);
    void remove_case_6(void* parent, void* child, void* trusted);

    void on_node_removed(void* parent, void* child, block_color deleted_color, void* trusted);
    void* find_rightmost(void* node);
    void remove_node(void* block, void* trusted);
}