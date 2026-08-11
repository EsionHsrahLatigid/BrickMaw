#pragma once

#include <exception>
#include <iostream>
#include <string>

namespace test_support
{
struct Failure final : std::exception
{
    explicit Failure(std::string m) : message(std::move(m)) {}
    const char* what() const noexcept override { return message.c_str(); }
    std::string message;
};

inline void check(bool condition, const std::string& message)
{
    if (!condition)
        throw Failure(message);
}

template <typename Func>
int run(const char* name, Func&& func)
{
    try
    {
        func();
        std::cout << name << " passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << name << " failed: " << e.what() << '\n';
        return 1;
    }
}
} // namespace test_support
