#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <memory>

#include <allocator_sorted_list.h>
#include <pp_allocator.h>

struct student
{
    int id;
    double average_score;
};

static void print_line()
{
    std::cout << "----------------------------------------\n";
}

int main()
{
    std::cout << "allocator_sorted_list demo started\n";
    print_line();

    std::unique_ptr<smart_mem_resource> resource(
        new allocator_sorted_list(
            8192,
            nullptr,
            allocator_with_fit_mode::fit_mode::first_fit));

    auto* fit_controller =
        dynamic_cast<allocator_with_fit_mode*>(resource.get());

    if (fit_controller == nullptr)
    {
        std::cout << "Failed to get fit mode controller\n";
        return 1;
    }

    std::cout << "Created allocator_sorted_list with 8192 bytes\n";
    std::cout << "Initial fit mode: first_fit\n";
    print_line();

    // 1. Ручное размещение одиночных объектов через pp_allocator
    pp_allocator<int> int_alloc(resource.get());
    pp_allocator<double> double_alloc(resource.get());
    pp_allocator<student> student_alloc(resource.get());

    int* number = int_alloc.new_object<int>(42);
    double* pi = double_alloc.new_object<double>(3.1415926535);
    student* st = student_alloc.new_object<student>(student{ 1, 4.75 });

    std::cout << "Single objects allocated in allocator memory:\n";
    std::cout << "int: " << *number << "\n";
    std::cout << "double: " << *pi << "\n";
    std::cout << "student: { id = " << st->id
        << ", average_score = " << st->average_score << " }\n";
    print_line();

    // 2. std::vector<int>
    std::vector<int, pp_allocator<int>> numbers(int_alloc);
    for (int i = 1; i <= 10; ++i)
    {
        numbers.push_back(i * 10);
    }

    std::cout << "std::vector<int> contents:\n";
    for (int x : numbers)
    {
        std::cout << x << ' ';
    }
    std::cout << "\n";
    print_line();

    // 3. std::list<std::string>
    pp_allocator<std::string> string_alloc(resource.get());
    std::list<std::string, pp_allocator<std::string>> words(string_alloc);

    words.push_back("allocator");
    words.push_back("sorted");
    words.push_back("list");
    words.push_back("demo");

    std::cout << "std::list<std::string> contents:\n";
    for (const auto& s : words)
    {
        std::cout << s << ' ';
    }
    std::cout << "\n";
    print_line();

    // 4. std::map<int, std::string>
    using map_value_type = std::pair<const int, std::string>;
    pp_allocator<map_value_type> map_alloc(resource.get());

    std::map<int, std::string, std::less<>, pp_allocator<map_value_type>> dictionary(map_alloc);

    dictionary[1] = "one";
    dictionary[2] = "two";
    dictionary[3] = "three";

    std::cout << "std::map<int, std::string> contents:\n";
    for (const auto& [key, value] : dictionary)
    {
        std::cout << key << " -> " << value << "\n";
    }
    print_line();

    // 5. Смена fit mode
    fit_controller->set_fit_mode(allocator_with_fit_mode::fit_mode::the_best_fit);
    std::cout << "Fit mode changed to: the_best_fit\n";

    std::vector<int, pp_allocator<int>> more_numbers(int_alloc);
    for (int i = 0; i < 5; ++i)
    {
        more_numbers.push_back(i * i);
    }

    std::cout << "Second vector after switching to the_best_fit:\n";
    for (int x : more_numbers)
    {
        std::cout << x << ' ';
    }
    std::cout << "\n";
    print_line();

    fit_controller->set_fit_mode(allocator_with_fit_mode::fit_mode::the_worst_fit);
    std::cout << "Fit mode changed to: the_worst_fit\n";

    std::list<student, pp_allocator<student>> group(student_alloc);
    group.push_back({ 2, 4.20 });
    group.push_back({ 3, 3.95 });
    group.push_back({ 4, 4.90 });

    std::cout << "std::list<student> contents:\n";
    for (const auto& x : group)
    {
        std::cout << "{ id = " << x.id
            << ", average_score = " << x.average_score
            << " }\n";
    }
    print_line();

    // 6. Освобождение вручную размещённых объектов
    student_alloc.delete_object(st);
    double_alloc.delete_object(pi);
    int_alloc.delete_object(number);

    std::cout << "Manually allocated single objects were deallocated\n";
    std::cout << "STL containers will release their memory automatically\n";
    print_line();

    std::cout << "Demo finished successfully\n";
    return 0;
}