#include <not_implemented.h>
#include "../include/allocator_sorted_list.h"

#include <new>
#include <cstring>
#include <utility>

namespace
{
    // -----------------------------------------------------------вспомогательные----------------------------------------------------------------------
    
    /*
        Приводит указатель к char* для байтовой арифметики -----------------------
    */
    
    inline char* bytes(void* p) noexcept
    {
        return static_cast<char*>(p);
    }

    inline const char* bytes(const void* p) noexcept
    {
        return static_cast<const char*>(p);
    }

    /*
        Доступ к метаданным аллокатора ------------------------------------------
    */

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

    inline void*& first_free_ref(void* trusted) {
        return *reinterpret_cast<void**>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t)
            + sizeof(std::mutex));
    }

    inline void* const& first_free_ref(const void* trusted) {
        return *reinterpret_cast<void* const*>(
            bytes(trusted)
            + sizeof(std::pmr::memory_resource*)
            + sizeof(allocator_with_fit_mode::fit_mode)
            + sizeof(size_t)
            + sizeof(std::mutex));
    }

    /*
        Доступ к метаданным блока ------------------------------------------------
    */

    inline void*& next_free_or_memory_begin_ref(void* block) noexcept
    {
        return *reinterpret_cast<void**>(block);
    }


    inline void* const& next_free_or_memory_begin_ref(const void* block) noexcept
    {
        return *reinterpret_cast<void* const*>(block);
    }

    inline size_t& block_size_ref(void* block) noexcept
    {
        return *reinterpret_cast<size_t*>(bytes(block) + sizeof(void*));
    }

    inline const size_t& block_size_ref(const void* block) noexcept
    {
        return *reinterpret_cast<const size_t*>(bytes(block) + sizeof(void*));
    }

    /*
        Константы ---------------------------------------------------------------
    */

    constexpr const size_t allocator_metadata_size = 
        sizeof(std::pmr::memory_resource*) 
        + sizeof(allocator_with_fit_mode::fit_mode) 
        + sizeof(size_t) 
        + sizeof(std::mutex) 
        + sizeof(void*);

    constexpr const size_t block_metadata_size = sizeof(void*) + sizeof(size_t);

    /*
        Границы ----------------------------------------------------------------
    */

    // Указатель на начало первого физического блока (сразу после метаданных аллокатора)
    inline char* first_block_ptr(void* trusted) noexcept
    {
        return bytes(trusted) + allocator_metadata_size;
    }

    inline const char* first_block_ptr(const void* trusted) noexcept
    {
        return bytes(trusted) + allocator_metadata_size;
    }

    // Указатель на конец доверенной области (за последним байтом пользовательских данных)
    inline char* memory_end_ptr(void* trusted)
    {
        return first_block_ptr(trusted) + total_space_ref(trusted);
    }

    inline const char* memory_end_ptr(const void* trusted)
    {
        return first_block_ptr(trusted) + total_space_ref(trusted);
    }

    // Указатель на начало пользовательских данных (после заголовка блока)
    inline char* user_memory_ptr(void* block) {
        return bytes(block) + block_metadata_size;
    }

    inline const char* user_memory_ptr(const void* block) {
        return bytes(block) + block_metadata_size;
    }

    //  Указатель на следующий физический блок (пропуская метаданные текущего и его данные)
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

    // Вставить блок в односвязный список свободных блоков, порядок по возрастанию адресов
    inline void insert_into_free_list_sorted(void* trusted, void* block) noexcept
    {
        void*& head = first_free_ref(trusted);

        if (head == nullptr || bytes(block) < bytes(head)) {
            next_free_or_memory_begin_ref(block) = head;
            head = block;
            return;
        }

        void* prev = head;
        void* cur = next_free_or_memory_begin_ref(head);

        while (cur != nullptr && bytes(cur) < bytes(block)) {
            prev = cur;
            cur = next_free_or_memory_begin_ref(cur);
        }

        next_free_or_memory_begin_ref(block) = cur;
        next_free_or_memory_begin_ref(prev) = block;
    }

    // После добавления блока в список свободных объединить его с соседними свободными блоками
    inline void merge_with_neighbours(void* trusted, void* block)
    {
        void* prev = nullptr;
        void* cur = first_free_ref(trusted);

        while (cur != nullptr && cur != block)
        {
            prev = cur;
            cur = next_free_or_memory_begin_ref(cur);
        }

        if (cur == nullptr)
            return;

        void* next = next_free_or_memory_begin_ref(cur);

        if (next != nullptr && next_physical_block(cur) == bytes(next)) {
            block_size_ref(cur) += block_metadata_size + block_size_ref(next);
            next_free_or_memory_begin_ref(cur) = next_free_or_memory_begin_ref(next);
        }

        if (prev != nullptr && next_physical_block(prev) == bytes(cur))
        {
            block_size_ref(prev) += block_metadata_size + block_size_ref(cur);
            next_free_or_memory_begin_ref(prev) = next_free_or_memory_begin_ref(cur);
        }
    }

    // Проверить, находится ли блок в списке свободных
    inline bool is_free_block(void* trusted, void* block)
    {
        void* cur = first_free_ref(trusted);
        while (cur != nullptr)
        {
            if (cur == block)
                return true;
            cur = next_free_or_memory_begin_ref(cur);
        }
        return false;
    }
}

//---------------------------------------------------------------------------основные-------------------------------------------------------------------------

allocator_sorted_list::~allocator_sorted_list()
{
    if (_trusted_memory == nullptr) return;
    auto* parent = parent_allocator_ref(_trusted_memory);
    size_t bytes_to_free = allocator_metadata_size + total_space_ref(_trusted_memory);

    mutex_ref(_trusted_memory).~mutex();
    parent->deallocate(_trusted_memory, bytes_to_free, alignof(std::max_align_t));

    _trusted_memory = nullptr;
}

allocator_sorted_list::allocator_sorted_list(allocator_sorted_list &&other) noexcept
{
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_sorted_list& allocator_sorted_list::operator=(allocator_sorted_list &&other) noexcept
{
    if (this == &other)
        return *this;

    this->~allocator_sorted_list();
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;

    return *this;
}

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource* parent_allocator,
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

    if (space_size >= block_metadata_size + 1)
    {
        void* first_block = first_block_ptr(_trusted_memory);
        first_free_ref(_trusted_memory) = first_block;
        next_free_or_memory_begin_ref(first_block) = nullptr;
        block_size_ref(first_block) = space_size - block_metadata_size;
    }
    else {
        first_free_ref(_trusted_memory) = nullptr;
    }
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr)
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));

    void* selected = nullptr;
    void* selected_prev = nullptr;

    void* prev = nullptr;
    void* cur = first_free_ref(_trusted_memory);

    auto mode = fit_mode_ref(_trusted_memory);

    while (cur != nullptr)
    {
        size_t cur_size = block_size_ref(cur);

        if (cur_size >= size)
        {
            if (mode == fit_mode::first_fit)
            {
                selected = cur;
                selected_prev = prev;
                break;
            }

            if (selected == nullptr) {
                selected = cur;
                selected_prev = prev;
            }
            else if (mode == fit_mode::the_best_fit && cur_size < block_size_ref(selected))
            {
                selected = cur;
                selected_prev = prev;
            }
            else if (mode == fit_mode::the_worst_fit && cur_size > block_size_ref(selected))
            {
                selected = cur;
                selected_prev = prev;
            }
        }

        prev = cur;
        cur = next_free_or_memory_begin_ref(cur);
    }

    if (selected == nullptr)
        throw std::bad_alloc();

    size_t selected_size = block_size_ref(selected);
    size_t remainder = selected_size - size;
    if (remainder >= block_metadata_size + 1)
    {
        void* new_free_block = bytes(selected) + block_metadata_size + size;

        next_free_or_memory_begin_ref(new_free_block) = next_free_or_memory_begin_ref(selected);
        block_size_ref(new_free_block) = remainder - block_metadata_size;

        if (selected_prev == nullptr)
            first_free_ref(_trusted_memory) = new_free_block;
        else
            next_free_or_memory_begin_ref(selected_prev) = new_free_block;

        block_size_ref(selected) = size;
        next_free_or_memory_begin_ref(selected) = _trusted_memory;
    }
    else
    {
        if (selected_prev == nullptr)
        {
            first_free_ref(_trusted_memory) = next_free_or_memory_begin_ref(selected);
        }
        else
        {
            next_free_or_memory_begin_ref(selected_prev) = next_free_or_memory_begin_ref(selected);
        }

        next_free_or_memory_begin_ref(selected) = _trusted_memory;
    }

    return user_memory_ptr(selected);
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
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

    first_free_ref(_trusted_memory) = nullptr;

    void* old_cur = first_free_ref(other._trusted_memory);
    void* new_prev = nullptr;

    while (old_cur != nullptr)
    {
        ptrdiff_t offset = bytes(old_cur) - bytes(other._trusted_memory);
        void* new_cur = bytes(_trusted_memory) + offset;

        if (new_prev == nullptr)
            first_free_ref(_trusted_memory) = new_cur;
        else
            next_free_or_memory_begin_ref(new_prev) = new_cur;

        new_prev = new_cur;
        old_cur = next_free_or_memory_begin_ref(old_cur);
    }

    if (new_prev != nullptr)
        next_free_or_memory_begin_ref(new_prev) = nullptr;

    for (char* cur = first_block_ptr(_trusted_memory);
        cur < memory_end_ptr(_trusted_memory);
        cur += block_metadata_size + block_size_ref(cur))
    {
        if (!is_free_block(_trusted_memory, cur))
            next_free_or_memory_begin_ref(cur) == _trusted_memory;
    }
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other)
{
    if (this == &other)
        return *this;

    *this = allocator_sorted_list(other);
    return *this;
    
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == dynamic_cast<const allocator_sorted_list*>(&other);
}

void allocator_sorted_list::do_deallocate_sm(void *at)
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

    if (next_free_or_memory_begin_ref(block) != _trusted_memory)
        throw std::logic_error("pointer does not belong to this allocator or block is already free");

    insert_into_free_list_sorted(_trusted_memory, block);
    merge_with_neighbours(_trusted_memory, block);
}

inline void allocator_sorted_list::set_fit_mode(allocator_with_fit_mode::fit_mode mode)
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    fit_mode_ref(_trusted_memory) = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_ref(_trusted_memory));
    return get_blocks_info_inner();
}


std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> vec;

    for (auto it = begin(); it != end(); it++)
    {
        vec.push_back({ it.size(), it.occupied() });
    }

    return vec;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    return sorted_free_iterator(first_free_ref(_trusted_memory));
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator();
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator();
}


bool allocator_sorted_list::sorted_free_iterator::operator==(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr != nullptr)
        _free_ptr = next_free_or_memory_begin_ref(_free_ptr);
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    auto copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    return _free_ptr == nullptr ? 0 : allocator_sorted_list::allocator_metadata_size + block_size_ref(_free_ptr);
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    return _free_ptr;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator()
    : _free_ptr(nullptr)
{
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void* trusted)
    : _free_ptr(trusted)
{
}

bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return !(*this == other);
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr == nullptr)
        return *this;

    char* next = next_physical_block(_current_ptr);

    if (_free_ptr == _current_ptr)
        _free_ptr = next_free_or_memory_begin_ref(_free_ptr);

    if (next >= memory_end_ptr(_trusted_memory))
        _current_ptr = nullptr;
    else
        _current_ptr = next;

    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    auto copy = *this;
    ++(*this);
    return copy;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    return _current_ptr == nullptr ? 0 : block_size_ref(_current_ptr);
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    return _current_ptr;
}

allocator_sorted_list::sorted_iterator::sorted_iterator()
    : _free_ptr(nullptr), _current_ptr(nullptr), _trusted_memory(nullptr)
{
}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted)
    : _free_ptr(nullptr), _current_ptr(nullptr), _trusted_memory(trusted)
{
    if (trusted == nullptr)
        return;

    _free_ptr = first_free_ref(trusted);
    _current_ptr = first_block_ptr(trusted);
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    if (_current_ptr == nullptr)
        return false;
    return _current_ptr != _free_ptr;
}
