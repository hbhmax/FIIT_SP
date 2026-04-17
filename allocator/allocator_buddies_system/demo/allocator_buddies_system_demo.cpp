#include <iostream>
#include <list>
#include <string>
#include <memory>
#include <cstddef>

#include <allocator_buddies_system.h>
#include <pp_allocator.h>
#include <allocator_test_utils.h>
#include <allocator_with_fit_mode.h>

namespace
{
    void print_blocks_state(const char* title, smart_mem_resource* resource)
    {
        std::cout << "\n=== " << title << " ===\n";

        auto* dbg = dynamic_cast<allocator_test_utils*>(resource);
        if (dbg == nullptr)
        {
            std::cout << "allocator_test_utils is not available\n";
            return;
        }

        const auto blocks = dbg->get_blocks_info();

        if (blocks.empty())
        {
            std::cout << "<empty>\n";
            return;
        }

        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            std::cout
                << "[" << i << "] "
                << (blocks[i].is_block_occupied ? "occupied" : "free")
                << ", size = " << blocks[i].block_size
                << '\n';
        }
    }

    template<typename T>
    void print_list(const std::list<T, pp_allocator<T>>& lst, const char* title)
    {
        std::cout << title << ": ";
        for (const auto& x : lst)
        {
            std::cout << x << ' ';
        }
        std::cout << '\n';
    }
}

int main()
{
    try
    {
        std::unique_ptr<smart_mem_resource> resource(
            new allocator_buddies_system(
                2048,
                nullptr,
                allocator_with_fit_mode::fit_mode::first_fit));

        auto* fit_subject = dynamic_cast<allocator_with_fit_mode*>(resource.get());
        if (fit_subject == nullptr)
        {
            std::cerr << "allocator_with_fit_mode is not available\n";
            return 1;
        }

        print_blocks_state("initial state", resource.get());

        // -----------------------------------------
        // 1. Raw allocations through memory_resource
        // -----------------------------------------
        void* block1 = resource->allocate(40, alignof(std::max_align_t));
        print_blocks_state("after allocate(40)", resource.get());

        void* block2 = resource->allocate(20, alignof(std::max_align_t));
        print_blocks_state("after allocate(20)", resource.get());

        resource->deallocate(block1, 40, alignof(std::max_align_t));
        print_blocks_state("after deallocate(block1)", resource.get());

        resource->deallocate(block2, 20, alignof(std::max_align_t));
        print_blocks_state("after deallocate(block2) and merge", resource.get());

        // -----------------------------------------
        // 2. Using pp_allocator<int> with std::list
        // -----------------------------------------
        pp_allocator<int> int_alloc(resource.get());
        std::list<int, pp_allocator<int>> numbers(int_alloc);

        for (int i = 1; i <= 8; ++i)
        {
            numbers.push_back(i * 10);
        }

        std::cout << '\n';
        print_list(numbers, "numbers");
        print_blocks_state("after std::list<int> allocations", resource.get());

        numbers.pop_front();
        numbers.pop_back();

        print_list(numbers, "numbers after pop_front/pop_back");
        print_blocks_state("after partial list deallocation", resource.get());

        numbers.clear();
        print_blocks_state("after numbers.clear()", resource.get());

        // -----------------------------------------
        // 3. Using pp_allocator<std::string>
        // -----------------------------------------
        pp_allocator<std::string> string_alloc(resource.get());
        std::list<std::string, pp_allocator<std::string>> words(string_alloc);

        words.emplace_back("buddy");
        words.emplace_back("allocator");
        words.emplace_back("demo");
        words.emplace_back("with");
        words.emplace_back("pp_allocator");

        std::cout << "\nwords: ";
        for (const auto& s : words)
        {
            std::cout << '"' << s << "\" ";
        }
        std::cout << '\n';

        print_blocks_state("after std::list<string> allocations", resource.get());

        words.clear();
        print_blocks_state("after words.clear()", resource.get());

        // -----------------------------------------
        // 4. Change fit mode through allocator_with_fit_mode
        // -----------------------------------------
        fit_subject->set_fit_mode(allocator_with_fit_mode::fit_mode::the_worst_fit);

        void* block3 = resource->allocate(16, alignof(std::max_align_t));
        void* block4 = resource->allocate(16, alignof(std::max_align_t));
        void* block5 = resource->allocate(16, alignof(std::max_align_t));

        print_blocks_state("after 3 allocations in worst_fit mode", resource.get());

        resource->deallocate(block4, 16, alignof(std::max_align_t));
        print_blocks_state("after deallocate(block4)", resource.get());

        resource->deallocate(block3, 16, alignof(std::max_align_t));
        resource->deallocate(block5, 16, alignof(std::max_align_t));
        print_blocks_state("after full cleanup and merge", resource.get());

        std::cout << "\nDemo finished successfully.\n";
    }
    catch (const std::bad_alloc& ex)
    {
        std::cerr << "bad_alloc: " << ex.what() << '\n';
        return 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "exception: " << ex.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "unknown exception\n";
        return 1;
    }

    return 0;
}