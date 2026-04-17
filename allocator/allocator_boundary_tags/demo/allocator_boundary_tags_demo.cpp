#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <memory>

#include "allocator_boundary_tags.h"
#include "pp_allocator.h"

namespace
{
    void print_separator(const std::string& title)
    {
        std::cout << "\n==== " << title << " ====\n";
    }

    void print_blocks(const allocator_test_utils& dbg)
    {
        const auto blocks = dbg.get_blocks_info();

        std::cout << "blocks count: " << blocks.size() << '\n';
        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            std::cout
                << "  [" << i << "] "
                << (blocks[i].is_block_occupied ? "occupied" : "free    ")
                << ", size = " << blocks[i].block_size << '\n';
        }
    }

    void print_ptr(const char* name, const void* ptr)
    {
        std::cout << std::left << std::setw(16) << name << " = " << ptr << '\n';
    }

    struct point
    {
        int x;
        int y;

        point(int x_, int y_) : x(x_), y(y_) {}
    };
}

int main()
{
    using fit_mode = allocator_with_fit_mode::fit_mode;

    std::cout << "Demo: allocator_boundary_tags + pp_allocator\n";
    std::cout << "Block metadata size in tests = "
        << sizeof(allocator_dbg_helper::block_size_t)
        + sizeof(allocator_dbg_helper::block_pointer_t) * 3
        << " bytes\n";

    allocator_boundary_tags arena(
        3000,
        nullptr,
        fit_mode::first_fit
    );

    auto* fit = dynamic_cast<allocator_with_fit_mode*>(&arena);
    auto* dbg = dynamic_cast<allocator_test_utils*>(&arena);

    if (fit == nullptr || dbg == nullptr)
    {
        std::cerr << "dynamic_cast failed\n";
        return 1;
    }

    pp_allocator<char> char_alloc(&arena);
    pp_allocator<int> int_alloc(&arena);
    pp_allocator<point> point_alloc(&arena);

    print_separator("Initial state");
    print_blocks(*dbg);

    print_separator("Allocate two raw char blocks via pp_allocator<char>");
    char* first = char_alloc.allocate(1000);
    char* second = char_alloc.allocate(0);

    print_ptr("first", first);
    print_ptr("second", second);
    print_blocks(*dbg);

    print_separator("Free first, then allocate 999 bytes again");
    char_alloc.deallocate(first, 1000);
    first = char_alloc.allocate(999);

    print_ptr("first(reused)", first);
    print_ptr("second", second);
    print_blocks(*dbg);

    print_separator("Allocate object with constructor via new_object");
    point* p = point_alloc.new_object<point>(10, 20);
    print_ptr("point", p);
    std::cout << "point value      = (" << p->x << ", " << p->y << ")\n";
    print_blocks(*dbg);

    print_separator("Allocate int array via allocate_object<int>");
    int* numbers = int_alloc.allocate_object<int>(8);
    for (int i = 0; i < 8; ++i)
    {
        int_alloc.construct(&numbers[i], i * 10);
    }

    std::cout << "numbers          = ";
    for (int i = 0; i < 8; ++i)
    {
        std::cout << numbers[i] << (i + 1 == 8 ? '\n' : ' ');
    }
    print_ptr("numbers", numbers);
    print_blocks(*dbg);

    print_separator("Free point and int array");
    point_alloc.delete_object(p);
    for (int i = 0; i < 8; ++i)
    {
        int_alloc.destroy(&numbers[i]);
    }
    int_alloc.deallocate_object(numbers, 8);
    print_blocks(*dbg);

    print_separator("Create holes and switch fit mode");
    char* a = char_alloc.allocate(200);
    char* b = char_alloc.allocate(300);
    char* c = char_alloc.allocate(400);

    print_ptr("a", a);
    print_ptr("b", b);
    print_ptr("c", c);
    print_blocks(*dbg);

    char_alloc.deallocate(b, 300);
    print_separator("After freeing middle block b");
    print_blocks(*dbg);

    fit->set_fit_mode(fit_mode::the_best_fit);
    char* best = char_alloc.allocate(120);
    print_separator("Allocate 120 bytes with best-fit");
    print_ptr("best", best);
    print_blocks(*dbg);

    char_alloc.deallocate(a, 200);
    char_alloc.deallocate(c, 400);
    char_alloc.deallocate(best, 120);
    print_separator("After freeing a/c/best");
    print_blocks(*dbg);

    print_separator("Use pp_allocator<int> with std::vector");
    std::vector<int, pp_allocator<int>> values((pp_allocator<int>(&arena)));
    for (int i = 1; i <= 12; ++i)
    {
        values.push_back(i * i);
    }

    std::cout << "vector values    = ";
    for (int value : values)
    {
        std::cout << value << ' ';
    }
    std::cout << '\n';
    print_blocks(*dbg);

    print_separator("Cleanup final raw blocks");
    char_alloc.deallocate(first, 999);
    char_alloc.deallocate(second, 0);
    print_blocks(*dbg);

    print_separator("Done");
    std::cout << "vector will be destroyed automatically at program end\n";

    return 0;
}
