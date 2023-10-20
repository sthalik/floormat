#pragma once
#include "compat/defs.hpp"
#include "tests.hpp"
#include "src/point.hpp"
#include <vector>
#include <variant>

namespace floormat::tests {

template<typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct path_test
{
    point from;
    std::vector<point> path;
    bool active = false;
};

using variant = std::variant<std::monostate, tests::path_test>;

} // namespace floormat::tests

namespace floormat {

struct tests_data final : tests_data_, tests::variant
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(tests_data);
    tests_data();
    ~tests_data() noexcept override;
    using tests::variant::operator=;
    //tests::variant& operator*();
    //tests::variant* operator->();
};

} // namespace floormat
