#pragma once
#include "tests.hpp"
#include "compat/defs.hpp"
#include "src/object-id.hpp"
#include "floormat/events.hpp"
#include <cr/StringView.h>
#include <cr/Pointer.h>

namespace floormat { struct app; struct Ns; }

namespace floormat::tests {

// A fast drag delivers several mouse-move events in one frame, and each is a distinct ray. The
// test used to keep only the last of them.
constexpr inline uint32_t max_pending_rays = 16;

struct base_test
{
    fm_DEFAULT_MOVE_(base_test);
    fm_DISABLE_COPY(base_test);

    virtual bool handle_key(app& a, const key_event& e, bool is_down) = 0;
    virtual bool handle_mouse_click(app& a, const mouse_button_event& e, bool is_down) = 0;
    virtual bool handle_mouse_move(app& a, const mouse_move_event& e) = 0;
    virtual void draw_overlay(app& a) = 0;
    virtual void draw_ui(app& a, float width) = 0;
    virtual void update_pre(app& a, const Ns& dt) = 0;
    virtual void update_post(app& a, const Ns& dt) = 0;

    enum class ValueType : uint8_t { none, ptr, usize, intptr, uintptr, i64, u64, i32, u32, f, b, ai32, au32, af };
    struct monostate {};

    struct Value {
        union {
            struct monostate empty = {};
            void* ptr;
            size_t usize;
            intptr_t intptr;
            uintptr_t uintptr;
            int64_t i64;
            uint64_t u64;
            int32_t i32;
            uint32_t u32;
            float f;
            bool b;
            int32_t ai32[4];
            uint32_t au32[4];
            float af[4];
        };

        ValueType type = ValueType::none;
    };

    // The driver cannot click an ImGui widget, and cover_test's octant selector is one. Steps it
    // and hands back the octants built so far as a bitmask. No other test has a selection.
    virtual Value advance(app& a, Value);

    virtual ~base_test() noexcept;

protected:
    base_test();
};

enum class Test : uint32_t {
    //todo add a speedometer overlay test
    none, path, raycast, grid, walk, hole, cover, COUNT,
};

struct tests_data final : tests_data_
{
    ~tests_data() noexcept override;

    void switch_to(Test i);

    static Pointer<base_test> make_test_none();
    static Pointer<base_test> make_test_path();
    static Pointer<base_test> make_test_raycast();
    static Pointer<base_test> make_test_grid();
    static Pointer<base_test> make_test_walk();
    static Pointer<base_test> make_test_hole();
    static Pointer<base_test> make_test_cover();

    Pointer<base_test> current_test;
    Test current_index = Test::none;

    struct test_tuple
    {
        StringView name;
        Test t;
        Pointer<base_test>(*ctor)();
    };

    static constexpr test_tuple fields[] = {
        { "None"_s,         Test::none,    make_test_none,   },
        { "Path search"_s,  Test::path,    make_test_path,   },
        { "Raycasting"_s,   Test::raycast, make_test_raycast },
        { "Grid"_s,         Test::grid,    make_test_grid    },
        { "Walking"_s,      Test::walk,    make_test_walk    },
        { "Hole"_s,         Test::hole,    make_test_hole    },
        { "Cover"_s,        Test::cover,   make_test_cover   },
    };
};

} // namespace floormat::tests
