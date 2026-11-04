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
constexpr inline u32 max_pending_rays = 16;

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

    enum class ValueType : u8 { none, ptr, usize, intptr, uintptr, i64, u64, i32, u32, f, b, ai32, au32, af };
    struct monostate {};

    struct Value {
        union {
            struct monostate empty = {};
            void* ptr;
            floormat::usize usize;
            iptr intptr;
            uptr uintptr;
            floormat::i64 i64;
            floormat::u64 u64;
            floormat::i32 i32;
            floormat::u32 u32;
            float f;
            bool b;
            floormat::i32 ai32[4];
            floormat::u32 au32[4];
            float af[4];
        };

        ValueType type = ValueType::none;
    };

    // Steps an ImGui widget the driver can't click. cover_test: the octant selector, returning the
    // octants so far as a bitmask. maze_test: build for 0, else solve. cover_ai_test, returning u32
    // unless noted: 0 Start and 3 Reset (npc id as u64, or none), 1 Stop, 2 "Other cover spots" with
    // a full search (spot count), 4 and 5 next and previous cover (spot index, or none), 6 Hold
    // (hold_phase), 7 "Move spots to cover's end" (1 when ticked), 8 "Cover div" set to au32[1] (the
    // div), 9 the box's search as au32: tiles listed, tiles in all, octant fills, and 1 while it runs,
    // 10 its spots as au32: seen from the player now, dropped as seen since the search started, listed,
    // 11 its center and octant as ai32, x, y and octant, then 1 while it runs.
    virtual Value advance(app& a, Value);
    // When the editor switches from the tests mode to another mode. Not when the test is replaced,
    // which runs its destructor.
    virtual void on_leave_tests_mode(app& a);

    virtual ~base_test() noexcept;

protected:
    base_test();
};

enum class Test : u32 {
    //todo add a speedometer overlay test
    none, path, raycast, grid, walk, hole, cover, maze, cover_ai, COUNT,
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
    static Pointer<base_test> make_test_maze();
    static Pointer<base_test> make_test_cover_ai();

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
        { "Maze"_s,         Test::maze,    make_test_maze    },
        { "Cover AI"_s,     Test::cover_ai, make_test_cover_ai },
    };
};

} // namespace floormat::tests
