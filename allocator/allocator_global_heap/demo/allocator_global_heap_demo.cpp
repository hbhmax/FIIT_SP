#include <iostream>
#include <memory>
#include <string>
#include <new>

#include <allocator_global_heap.h>
#include <pp_allocator.h>

struct my_struct
{
    int id;
    double value;
    char symbol;
};

int main()
{
    std::unique_ptr<smart_mem_resource> resource(new allocator_global_heap());
    pp_allocator<std::byte> alloc(resource.get());

    // 1. int
    int* p_int = alloc.new_object<int>(42);

    // 2. double
    double* p_double = alloc.new_object<double>(3.14159);

    // 3. char
    char* p_char = alloc.new_object<char>('A');

    // 4. пользовательская структура
    my_struct* p_struct = alloc.allocate_object<my_struct>();
    std::construct_at(p_struct, my_struct{ 7, 12.5, 'Z' });

    // 5. массив int
    int* arr = alloc.allocate_object<int>(5);
    for (int i = 0; i < 5; ++i)
    {
        arr[i] = i * 10;
    }

    // 6. std::string
    std::string* p_string = alloc.allocate_object<std::string>();
    std::construct_at(p_string, "hello allocator");

    // вывод результатов
    std::cout << "int: " << *p_int << '\n';
    std::cout << "double: " << *p_double << '\n';
    std::cout << "char: " << *p_char << '\n';

    std::cout << "my_struct: { id = " << p_struct->id
        << ", value = " << p_struct->value
        << ", symbol = " << p_struct->symbol << " }\n";

    std::cout << "array: ";
    for (int i = 0; i < 5; ++i)
    {
        std::cout << arr[i] << ' ';
    }
    std::cout << '\n';

    std::cout << "string: " << *p_string << '\n';

    // освобождение памяти
    alloc.delete_object(p_int);
    alloc.delete_object(p_double);
    alloc.delete_object(p_char);

    std::destroy_at(p_struct);
    alloc.deallocate_object(p_struct);

    alloc.deallocate_object(arr, 5);

    std::destroy_at(p_string);
    alloc.deallocate_object(p_string);

    return 0;
}