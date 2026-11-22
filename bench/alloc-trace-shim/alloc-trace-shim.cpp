#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
void* __libc_malloc(size_t);
void* __libc_calloc(size_t, size_t);
void* __libc_realloc(void*, size_t);
void __libc_free(void*);
void* __libc_memalign(size_t, size_t);
}

namespace {

struct trace_record { uint64_t ptr, old_ptr, bytes; uint32_t align, kind; };
static_assert(sizeof(trace_record) == 32);

enum : uint32_t { kind_new, kind_delete, kind_malloc, kind_calloc, kind_realloc, kind_free };

constexpr size_t max_records = (size_t)1 << 25;
trace_record* records;
std::atomic<size_t> num_records;
std::atomic<bool> is_tracing;
pthread_t traced_thread;
char trace_path[4096];

inline void record(uint32_t kind, const void* p, const void* old, size_t bytes, size_t align)
{
    if (!is_tracing.load(std::memory_order_acquire) || !pthread_equal(pthread_self(), traced_thread))
        return;
    const size_t i = num_records.fetch_add(1, std::memory_order_relaxed);
    if (i >= max_records)
        abort();
    records[i] = {(uint64_t)p, (uint64_t)old, (uint64_t)bytes, (uint32_t)align, kind};
}

void* new_or_throw(size_t bytes, size_t align)
{
    void* const p = align ? __libc_memalign(align, bytes ? bytes : 1) : __libc_malloc(bytes ? bytes : 1);
    if (!p)
        throw std::bad_alloc{};
    record(kind_new, p, nullptr, bytes, align);
    return p;
}

void* new_or_null(size_t bytes, size_t align) noexcept
{
    void* const p = align ? __libc_memalign(align, bytes ? bytes : 1) : __libc_malloc(bytes ? bytes : 1);
    if (p)
        record(kind_new, p, nullptr, bytes, align);
    return p;
}

void delete_buffer(void* p, size_t bytes, size_t align) noexcept
{
    record(kind_delete, p, nullptr, bytes, align);
    __libc_free(p);
}

} // namespace

extern "C" __attribute__((visibility("default"))) void fm_alloc_trace_start(const char* path)
{
    if (!records)
    {
        void* const m = mmap(nullptr, max_records * sizeof(trace_record), PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (m == MAP_FAILED)
            abort();
        records = (trace_record*)m;
    }
    const size_t len = strlen(path);
    if (len >= sizeof trace_path)
        abort();
    memcpy(trace_path, path, len + 1);
    num_records.store(0, std::memory_order_relaxed);
    traced_thread = pthread_self();
    is_tracing.store(true, std::memory_order_release);
}

extern "C" __attribute__((visibility("default"))) void fm_alloc_trace_stop()
{
    is_tracing.store(false, std::memory_order_release);
    const int fd = open(trace_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        abort();
    const char* data = (const char*)records;
    size_t left = num_records.load(std::memory_order_relaxed) * sizeof(trace_record);
    while (left)
    {
        const ssize_t n = write(fd, data, left);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            abort();
        data += n;
        left -= (size_t)n;
    }
    close(fd);
}

extern "C" void* malloc(size_t bytes)
{
    void* const p = __libc_malloc(bytes);
    record(kind_malloc, p, nullptr, bytes, 0);
    return p;
}

extern "C" void* calloc(size_t n, size_t size)
{
    void* const p = __libc_calloc(n, size);
    record(kind_calloc, p, nullptr, n * size, 0);
    return p;
}

extern "C" void* realloc(void* old, size_t bytes)
{
    void* const p = __libc_realloc(old, bytes);
    record(kind_realloc, p, old, bytes, 0);
    return p;
}

extern "C" void* reallocarray(void* old, size_t n, size_t size)
{
    size_t bytes;
    if (__builtin_mul_overflow(n, size, &bytes))
    {
        errno = ENOMEM;
        return nullptr;
    }
    return realloc(old, bytes);
}

extern "C" void free(void* p)
{
    record(kind_free, p, nullptr, 0, 0);
    __libc_free(p);
}

extern "C" void* memalign(size_t align, size_t bytes)
{
    void* const p = __libc_memalign(align, bytes);
    record(kind_malloc, p, nullptr, bytes, align);
    return p;
}

extern "C" void* aligned_alloc(size_t align, size_t bytes)
{
    return memalign(align, bytes);
}

extern "C" int posix_memalign(void** out, size_t align, size_t bytes)
{
    void* const p = __libc_memalign(align, bytes);
    if (!p)
        return ENOMEM;
    record(kind_malloc, p, nullptr, bytes, align);
    *out = p;
    return 0;
}

extern "C" void* valloc(size_t bytes)
{
    return memalign((size_t)sysconf(_SC_PAGESIZE), bytes);
}

void* operator new(size_t n) { return new_or_throw(n, 0); }
void* operator new[](size_t n) { return new_or_throw(n, 0); }
void* operator new(size_t n, std::align_val_t a) { return new_or_throw(n, (size_t)a); }
void* operator new[](size_t n, std::align_val_t a) { return new_or_throw(n, (size_t)a); }
void* operator new(size_t n, const std::nothrow_t&) noexcept { return new_or_null(n, 0); }
void* operator new[](size_t n, const std::nothrow_t&) noexcept { return new_or_null(n, 0); }
void* operator new(size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return new_or_null(n, (size_t)a); }
void* operator new[](size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return new_or_null(n, (size_t)a); }
void operator delete(void* p) noexcept { delete_buffer(p, 0, 0); }
void operator delete[](void* p) noexcept { delete_buffer(p, 0, 0); }
void operator delete(void* p, size_t n) noexcept { delete_buffer(p, n, 0); }
void operator delete[](void* p, size_t n) noexcept { delete_buffer(p, n, 0); }
void operator delete(void* p, std::align_val_t a) noexcept { delete_buffer(p, 0, (size_t)a); }
void operator delete[](void* p, std::align_val_t a) noexcept { delete_buffer(p, 0, (size_t)a); }
void operator delete(void* p, size_t n, std::align_val_t a) noexcept { delete_buffer(p, n, (size_t)a); }
void operator delete[](void* p, size_t n, std::align_val_t a) noexcept { delete_buffer(p, n, (size_t)a); }
void operator delete(void* p, const std::nothrow_t&) noexcept { delete_buffer(p, 0, 0); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { delete_buffer(p, 0, 0); }
void operator delete(void* p, std::align_val_t a, const std::nothrow_t&) noexcept { delete_buffer(p, 0, (size_t)a); }
void operator delete[](void* p, std::align_val_t a, const std::nothrow_t&) noexcept { delete_buffer(p, 0, (size_t)a); }
