#pragma once
#include "defs.hpp"
#include "inplace-function-fwd.hpp"
#include "integer-types.hpp"
#include <type_traits>

namespace floormat {

class ThreadPool final
{
    struct state;
    state* _state;

    static void run_worker(state& s, u32 worker_index) noexcept;
    void parallel_for_impl(u32 count, u32 grain_size,
                           void (*run)(void* body, u32 begin, u32 end, u32 worker_index) noexcept, void* body) noexcept;

public:
    // num_threads counts the constructing thread. A caller that doesn't run jobs needs num_threads >= 2.
    explicit ThreadPool(u32 num_threads, bool is_caller_running_jobs = true) noexcept;
    // Returns after every job submitted before it has run.
    ~ThreadPool() noexcept;
    fm_DISABLE_MOVE_COPY(ThreadPool);

    u32 num_threads() const noexcept;
    // Returns ceil(count / (num_threads() * 8)), at least 1.
    u32 default_grain_size(u32 count) const noexcept;
    // Any thread may call it.
    bool is_running() const noexcept;

    // Calls body(begin, end, worker_index) once per grain_size range of [0, count). worker_index is 0 on the
    // calling thread. Only the constructing thread may call it, and never from inside a job.
    template<typename F> void parallel_for(u32 count, u32 grain_size, F&& body) noexcept;
    // Any thread may call it. parallel_for() doesn't wait for the job. Needs num_threads >= 2.
    void submit(inplace_function<void(u32 worker_index) noexcept, 48> job) noexcept;
};

template<typename F> void ThreadPool::parallel_for(u32 count, u32 grain_size, F&& body) noexcept
{
    using B = std::remove_reference_t<F>;
    parallel_for_impl(count, grain_size, [](void* p, u32 begin, u32 end, u32 worker_index) noexcept {
        (*static_cast<B*>(p))(begin, end, worker_index);
    }, const_cast<void*>(static_cast<const void*>(__builtin_addressof(body))));
}

} // namespace floormat
