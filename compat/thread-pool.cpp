#include "thread-pool.hpp"
#include "arch.hpp"
#include "assert.hpp"
#include "atomic.hpp"
#include "chase-lev-deque.inl"
#include "event-count.hpp"
#include "inplace-function.hpp"
#include "nanosecond.inl"
#include "timer.hpp"
#include "stl/thread.hpp"
#include <mutex>
#include <vector>

namespace floormat {

struct ThreadPool::state
{
    struct job final { void (*run)(void*, u32, u32, u32) noexcept; void* body; u32 begin, end; };

    // Declared first, so that it outlives the deque.
    ChaseLevBufferPool buffers;
    // The constructing thread pushes and takes. The pool's threads only steal.
    ChaseLevDeque<job> deque{buffers};
    alignas(64) EventCount idle_workers;
    // Counts parallel_for() jobs only, never submitted ones.
    alignas(64) volatile u32 num_unfinished_jobs = 0;
    // Written under shared_queue_mutex, read without it.
    alignas(64) volatile u32 num_shared_queue_jobs = 0;
    volatile u8 is_stop_requested = 0;
    std::mutex shared_queue_mutex;
    // Corrade's Array asserts through CorradeUtility, which floormat-test-atomic doesn't link.
    std::vector<inplace_function<void(u32 worker_index) noexcept, 48>> shared_queue;
    u32 shared_queue_head = 0;
    std::vector<job> parallel_for_jobs;
    std::vector<std::thread> threads;
    std::thread::id constructing_thread_id;
    inplace_function<void(u32 worker_index) noexcept, 48> start_handler;
    Ns idle_spin_duration;
    u32 num_threads = 0;
    bool is_caller_running_jobs = false;
};

// clang's -Wthread-safety-negative flags a member function of state that locks state's own mutex.
void ThreadPool::run_worker(state& s, u32 worker_index) noexcept
{
    set_fp_mask();
    if (s.start_handler)
        s.start_handler(worker_index);

    auto try_run_job = [&s, worker_index] {
        if (state::job* j = s.deque.steal())
        {
            j->run(j->body, j->begin, j->end, worker_index);
            u32 old = atomic_fetch_sub(&s.num_unfinished_jobs, 1u, memory_order::acq_rel);
            fm_assert(old != 0);
            if (old == 1)
                atomic_notify_all(&s.num_unfinished_jobs);
            return true;
        }
        else if (atomic_load(&s.num_shared_queue_jobs, memory_order::relaxed) != 0)
        {
            inplace_function<void(u32 worker_index) noexcept, 48> submitted_job;
            s.shared_queue_mutex.lock();
            if (s.shared_queue_head < s.shared_queue.size())
            {
                submitted_job = move(s.shared_queue[s.shared_queue_head]);
                ++s.shared_queue_head;
                if (s.shared_queue_head == s.shared_queue.size())
                {
                    s.shared_queue.clear();
                    s.shared_queue_head = 0;
                }
                atomic_store(&s.num_shared_queue_jobs, (u32)s.shared_queue.size() - s.shared_queue_head, memory_order::relaxed);
            }
            s.shared_queue_mutex.unlock();
            if (submitted_job)
            {
                submitted_job(worker_index);
                return true;
            }
            else
                return false;
        }
        else
            return false;
    };
    auto try_run_job_while_spinning = [&s, &try_run_job] {
        const Time spin_start = Time::now();
        while (Time::now() - spin_start < s.idle_spin_duration)
        {
            cpu_relax();
            if (try_run_job())
                return true;
        }
        return false;
    };

    for (;;)
    {
        if (try_run_job())
            continue;
        if (s.idle_spin_duration != Ns{} && try_run_job_while_spinning())
            continue;
        // The stop flag is loaded after prepare_wait(), or its wake-up is lost, and before the job check, or a job
        // submitted just before the stop is dropped.
        u32 key = s.idle_workers.prepare_wait();
        const bool is_stopping = atomic_load(&s.is_stop_requested, memory_order::acquire) != 0;
        if (!s.deque.empty() || atomic_load(&s.num_shared_queue_jobs, memory_order::relaxed) != 0)
            s.idle_workers.cancel_wait();
        else if (is_stopping)
        {
            s.idle_workers.cancel_wait();
            break;
        }
        else
            s.idle_workers.commit_wait(key);
    }
}

ThreadPool::ThreadPool(thread_pool_options options) noexcept:
    _state{new state}
{
    fm_assert(options.num_threads >= 1);
    auto& s = *_state;
    s.constructing_thread_id = std::this_thread::get_id();
    s.start_handler = move(options.start_handler);
    s.idle_spin_duration = options.idle_spin_usec * Microsecond;
    s.num_threads = options.num_threads;
    s.is_caller_running_jobs = options.is_caller_running_jobs;
    s.shared_queue.reserve(16);
    s.threads.reserve(options.num_threads - 1);
    for (u32 i = 1; i < options.num_threads; ++i)
        s.threads.emplace_back([&s, i] { run_worker(s, i); });
}

ThreadPool::ThreadPool(u32 num_threads, bool is_caller_running_jobs) noexcept:
    ThreadPool{thread_pool_options{.num_threads = num_threads, .is_caller_running_jobs = is_caller_running_jobs}}
{
}

ThreadPool::~ThreadPool() noexcept
{
    auto& s = *_state;
    fm_assert(!is_running());
    atomic_store(&s.is_stop_requested, u8{1}, memory_order::release);
    s.idle_workers.notify_all();
    for (auto& t : s.threads)
        t.join();
    delete _state;
}

u32 ThreadPool::num_threads() const noexcept
{
    return _state->num_threads;
}

u32 ThreadPool::default_grain_size(u32 count) const noexcept
{
    const u32 num_jobs = _state->num_threads * 8;
    return count ? (count - 1) / num_jobs + 1 : 1;
}

bool ThreadPool::is_running() const noexcept
{
    return atomic_load(&_state->num_unfinished_jobs, memory_order::relaxed) != 0;
}

u32 ThreadPool::num_waiting_workers() const noexcept
{
    return _state->idle_workers.num_waiters();
}

void ThreadPool::parallel_for_impl(u32 count, u32 grain_size,
                                   void (*run)(void* body, u32 begin, u32 end, u32 worker_index) noexcept,
                                   void* body) noexcept
{
    auto& s = *_state;
    fm_assert(std::this_thread::get_id() == s.constructing_thread_id);
    fm_assert(!is_running());
    fm_assert(grain_size >= 1);
    fm_assert(s.is_caller_running_jobs || s.num_threads >= 2);
    if (count == 0)
        return;

    const u32 num_jobs = count / grain_size + (count % grain_size != 0);
    if (num_jobs > s.parallel_for_jobs.size())
        s.parallel_for_jobs = std::vector<state::job>(num_jobs + num_jobs / 2);
    for (u32 i = 0; i < num_jobs; ++i)
    {
        const u32 begin = i * grain_size;
        const u32 end = count - begin > grain_size ? begin + grain_size : count;
        s.parallel_for_jobs[i] = { run, body, begin, end };
    }

    atomic_store(&s.num_unfinished_jobs, num_jobs, memory_order::relaxed);
    for (u32 i = 0; i < num_jobs; ++i)
        s.deque.push(&s.parallel_for_jobs[i]);
    // GenMC finds a lost wake-up with the waiter count loaded before the pushes.
    s.idle_workers.notify_all();

    if (s.is_caller_running_jobs)
        while (state::job* j = s.deque.take())
        {
            j->run(j->body, j->begin, j->end, 0);
            u32 old = atomic_fetch_sub(&s.num_unfinished_jobs, 1u, memory_order::acq_rel);
            fm_assert(old != 0);
        }

    // Acquire, with the acq_rel decrements, makes the jobs' writes visible to the caller.
    for (u32 v; (v = atomic_load(&s.num_unfinished_jobs, memory_order::acquire)) != 0; )
        atomic_wait(&s.num_unfinished_jobs, v, memory_order::acquire);
}

void ThreadPool::submit(inplace_function<void(u32 worker_index) noexcept, 48> job) noexcept
{
    auto& s = *_state;
    fm_assert(s.num_threads >= 2);
    fm_assert(job);
    s.shared_queue_mutex.lock();
    s.shared_queue.push_back(move(job));
    atomic_store(&s.num_shared_queue_jobs, (u32)s.shared_queue.size() - s.shared_queue_head, memory_order::relaxed);
    s.shared_queue_mutex.unlock();
    s.idle_workers.notify_one();
}

} // namespace floormat
