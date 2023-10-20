#pragma once
#include "tests.hpp"
#include "compat/defs.hpp"
#include "src/point.hpp"
#include "floormat/events.hpp"
#include <Corrade/Containers/StringView.h>
#include <vector>
#include <variant>

namespace floormat { struct app; }

namespace floormat::tests {

template<typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct base_test
{
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(base_test);
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(base_test);

    virtual bool handle_key(app& a, const key_event& e) = 0;
    virtual bool handle_mouse_click(app& a, const mouse_button_event& e) = 0;
    virtual bool handle_mouse_move(app& a, const mouse_move_event& e) = 0;
    virtual void draw_overlay(app& a) = 0;
    virtual void update_pre(app& a) = 0;
    virtual void update_post(app& a) = 0;

    virtual ~base_test() noexcept;

protected:
    base_test();
};

struct path_test : base_test
{
    bool handle_key(app& a, const key_event& e) override;
    bool handle_mouse_click(app& a, const mouse_button_event& e) override;
    bool handle_mouse_move(app& a, const mouse_move_event& e) override;
    void draw_overlay(app& a) override;
    void update_pre(app& a) override;
    void update_post(app& a) override;

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

    struct pair { StringView str; size_t index; };

    static constexpr inline pair fields[] = {
        { "Path"_s, 1 },
    };

    void switch_to(size_t i);
};

} // namespace floormat
