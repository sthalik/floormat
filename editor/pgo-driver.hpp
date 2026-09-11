#pragma once
#include "compat/defs.hpp"
#include "floormat/events.hpp"
#include "src/timer.hpp"
#include "src/raycast-diag.hpp"
#include <array>
#include <coroutine>

namespace floormat::pgo {

// more rays makes the screen illegible
constexpr inline uint32_t max_rays_per_frame = 32;

struct pause final { uint32_t num_frames = 1; };

struct [[nodiscard]] task final
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct sentinel final {};

    struct iterator final
    {
        handle_type h;

        bool operator==(sentinel) const noexcept { return !h || h.done(); }
        iterator& operator++();
        const pause& operator*() const noexcept;
    };

    task() noexcept = default;
    explicit task(handle_type h) noexcept: h{h} {}
    ~task() noexcept;

    fm_DISABLE_COPY(task);
    task(task&& other) noexcept;
    task& operator=(task&& other) noexcept;

    bool done() const noexcept { return !h || h.done(); }
    void start();
    void tick();

    // The loop form, for a check between a sub-task's yields. A body that doesn't re-yield
    // loses the sub's frame counts, and an empty one runs the whole sub-task in one tick.
    iterator begin();
    static sentinel end() noexcept;

private:
    handle_type h = {};
};

struct task::promise_type final
{
    // Which one the runner reads depends on the form. Under co_await it counts the sub's
    // `frames_to_wait` down. Under the loop form the caller re-yields the sub's `current`.
    pause current;
    uint32_t frames_to_wait = 0;
    // Default-constructed is done(), so no separate flag says whether one is active.
    task sub;

    task get_return_object();
    static std::suspend_always initial_suspend() noexcept;
    static std::suspend_always final_suspend() noexcept;
    void return_void() noexcept;
    [[noreturn]] static void unhandled_exception();

    std::suspend_always yield_value(pause p) noexcept;

    struct sub_awaiter final
    {
        promise_type& p;

        // A sub-task with no suspension point is already done, and suspending on it would cost
        // a frame the loop form never costs.
        bool await_ready() const noexcept;
        static void await_suspend(handle_type) noexcept;
        void await_resume() const noexcept { p.sub = {}; }
    };

    // Only reached by co_await, never by the one a co_yield expands to ([expr.await]/3.2).
    sub_awaiter await_transform(task&& t);
};



struct state final
{
    task scene_task;
    Time scene_started;
    uint32_t scene_index = 0;
    uint32_t pass_index = 0;
    uint32_t frames_run = 0;
    uint32_t scene_first_frame = 0;
    // All ones means --driver-scenes was not given.
    uint32_t scene_mask = (uint32_t)-1;
    // scene_raycast raycasts and draws its own sweep. Going through the raycast test would have
    // shown one ray of the batch, since a test keeps a single result and the driver fires many.
    std::array<rc::raycast_result_s, max_rays_per_frame> rays;
    uint32_t num_rays = 0;
    // One per ray of the batch. raycast_with_diag() clears and refills whichever it is handed,
    // so a single one would leave the cells of 31 of the 32 rays undrawn.
    std::array<rc::raycast_diag_s, max_rays_per_frame> diags;
    Array<point> route;
    // get_key_modifiers() returns this while running, so a physically-held Ctrl can't change
    // what a scene does.
    int mods = 0;
    mouse_button held_buttons = mouse_button_none;
    bool running = false;
};

} // namespace floormat::pgo
