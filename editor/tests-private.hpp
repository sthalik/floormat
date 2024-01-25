#pragma once
#include "tests.hpp"
#include "compat/defs.hpp"
#include "src/point.hpp"
#include "src/object-id.hpp"
#include "floormat/events.hpp"
#include <Corrade/Containers/StringView.h>
#include <memory>
#include <vector>

namespace floormat { struct app; }

namespace floormat::tests {

struct base_test
{
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(base_test);
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(base_test);

    virtual bool handle_key(app& a, const key_event& e, bool is_down) = 0;
    virtual bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) = 0;
    virtual bool handle_mouse_move(app& a, const mouse_move_event& e) = 0;
    virtual void draw_overlay(app& a) = 0;
    virtual void draw_ui(app& a, float width) = 0;
    virtual void update_pre(app& a) = 0;
    virtual void update_post(app& a) = 0;

    virtual ~base_test() noexcept;

protected:
    base_test();
};

void label_left(StringView label, float width);

enum class Test : uint32_t {
    none, path, COUNT,
};

struct tests_data final : tests_data_
{
    ~tests_data() noexcept override;

    void switch_to(Test i);

    static std::unique_ptr<base_test> make_test_none();
    static std::unique_ptr<base_test> make_test_path();

    std::unique_ptr<base_test> current_test;
    Test current_index = Test::none;

    struct test_tuple
    {
        StringView name;
        Test t;
        std::unique_ptr<base_test>(*ctor)();
    };

    static constexpr test_tuple fields[] = {
        { "None"_s, Test::none, &tests_data::make_test_none, },
        { "Path"_s, Test::path, &tests_data::make_test_path, },
    };
};

} // namespace floormat::tests
