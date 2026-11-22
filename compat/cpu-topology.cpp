#include "cpu-topology.hpp"
#include "erase-if.hpp"
#include "stl/thread.hpp"
#include <bit>
#include <cr/StringView.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cr/Array.h>
#elif defined __linux__
#include "any-of.hpp"
#include "format.inl"
#include <cr/Optional.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#endif

namespace floormat {

namespace {

struct cpu_topology
{
    inplace_array<cpu_core, 64> cores;
    inplace_array<l3_cache, 64> l3_caches;
};

u64 hardware_concurrency_mask() noexcept
{
    const u32 n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : n >= 64 ? ~u64{0} : (u64{1} << n) - 1;
}

#ifdef _WIN32

cpu_topology query_cpu_topology(u64) noexcept
{
    GROUP_AFFINITY group;
    DWORD size = 0;
    if (!GetThreadGroupAffinity(GetCurrentThread(), &group) ||
        GetLogicalProcessorInformationEx(RelationAll, nullptr, &size) || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return {};
    Array<u64> buf{NoInit, (size + 7) / 8};
    if (!GetLogicalProcessorInformationEx(RelationAll, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)buf.data(), &size))
        return {};

    cpu_topology t;
    const char* const records = (const char*)buf.data();
    for (DWORD pos = 0; pos < size; )
    {
        const auto& r = *(const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(const void*)(records + pos);
        if (r.Size == 0 || r.Size > size - pos)
        {
            t = {};
            break;
        }
        if (r.Relationship == RelationProcessorCore)
        {
            const GROUP_AFFINITY& mask = r.Processor.GroupMask[0];
            if (mask.Group == group.Group && t.cores.size() < t.cores.capacity())
                t.cores.push_back({mask.Mask, r.Processor.EfficiencyClass});
        }
        else if (r.Relationship == RelationCache && r.Cache.Level == 3)
        {
            const GROUP_AFFINITY& mask = r.Cache.GroupMask;
            if (mask.Group == group.Group && t.l3_caches.size() < t.l3_caches.capacity())
                t.l3_caches.push_back({mask.Mask, r.Cache.CacheSize});
        }
        pos += r.Size;
    }
    return t;
}

#elif defined __linux__

// Returns the file's contents read into buf, or nothing when it can't be read or doesn't fit.
StringView read_sysfs_file(const char* path, ArrayView<char> buf) noexcept
{
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return {};
    usize size = 0;
    ssize_t n = 0;
    while (size < buf.size() && (n = ::read(fd, buf.data() + size, buf.size() - size)) > 0)
        size += (usize)n;
    ::close(fd);
    if (n < 0 || size == buf.size())
        return {};
    else
        return StringView{buf.data(), size};
}

Optional<u64> parse_decimal(StringView s) noexcept
{
    s = s.trimmedSuffix();
    if (s.isEmpty() || s.size() > 19)
        return {};
    u64 value = 0;
    for (char c : s)
        if (c < '0' || c > '9')
            return {};
        else
            value = value * 10 + u64(c - '0');
    return value;
}

// Returns the bytes of a sysfs cache size such as "32768K", or 0 when it doesn't parse.
u32 parse_cache_size(StringView s) noexcept
{
    s = s.trimmedSuffix();
    if (s.isEmpty())
        return 0;
    const u64 unit = s.back() == 'K' ? u64{1} << 10 : s.back() == 'M' ? u64{1} << 20 : 0;
    const auto number = parse_decimal(s.exceptSuffix(1));
    if (!unit || !number || *number > u32(-1) / unit)
        return 0;
    else
        return u32(*number * unit);
}

cpu_topology query_cpu_topology(u64 affinity_mask) noexcept
{
    // Its sysfs id, or its shared_cpu_list where it has no id.
    struct l3_cache_key
    {
        u64 id, shared_cpu_mask;
        bool has_id;

        bool operator==(const l3_cache_key&) const = default;
    };

    cpu_topology t;
    inplace_array<l3_cache_key, 64> l3_cache_keys;
    char path[96], buf[1024];
    for (u64 m = affinity_mask; m; m &= m - 1)
    {
        const auto cpu = (u32)std::countr_zero(m);
        snformat(path, "/sys/devices/system/cpu/cpu{}/topology/thread_siblings_list", cpu);
        const u64 siblings = parse_cpu_list(read_sysfs_file(path, buf));
        if (siblings && !any_of(t.cores, [=](const cpu_core& c) { return c.logical_cpu_mask == siblings; }))
            t.cores.push_back({siblings, 0});

        const auto read_cache_file = [&](u32 index, const char* name) {
            snformat(path, "/sys/devices/system/cpu/cpu{}/cache/index{}/{}", cpu, index, name);
            return read_sysfs_file(path, buf);
        };
        for (u32 index = 0; ; index++)
        {
            const StringView level_file = read_cache_file(index, "level");
            if (level_file.isEmpty())
                break;
            const auto level = parse_decimal(level_file);
            if (!level || *level != 3)
                continue;

            l3_cache_key key{};
            if (const auto id = parse_decimal(read_cache_file(index, "id")))
                key = {*id, 0, true};
            else
                key.shared_cpu_mask = parse_cpu_list(read_cache_file(index, "shared_cpu_list"));
            const u32 size_bytes = parse_cache_size(read_cache_file(index, "size"));
            if (!key.has_id && !key.shared_cpu_mask)
                continue;

            u32 i = 0;
            while (i < l3_cache_keys.size() && !(l3_cache_keys[i] == key))
                i++;
            if (i < l3_cache_keys.size())
                t.l3_caches[i].logical_cpu_mask |= u64{1} << cpu;
            else if (i < l3_cache_keys.capacity())
            {
                l3_cache_keys.push_back(key);
                t.l3_caches.push_back({u64{1} << cpu, size_bytes});
            }
        }
    }
    return t;
}

#else

cpu_topology query_cpu_topology(u64) noexcept { return {}; }

#endif

} // namespace

thread_pool_cpus pick_thread_pool_cpus(ArrayView<const cpu_core> cores, ArrayView<const l3_cache> l3_caches,
                                       u64 affinity_mask) noexcept
{
    if (!affinity_mask)
        return {};

    inplace_array<cpu_core, 64> allowed_cores;
    u8 max_efficiency_class = 0;
    for (const cpu_core& c : cores)
        if (const u64 mask = c.logical_cpu_mask & affinity_mask)
        {
            allowed_cores.push_back({mask, c.efficiency_class});
            if (c.efficiency_class > max_efficiency_class)
                max_efficiency_class = c.efficiency_class;
        }
    if (allowed_cores.empty())
        for (u64 m = affinity_mask; m; m &= m - 1)
            allowed_cores.push_back({u64{1} << std::countr_zero(m), 0});
    erase_if(allowed_cores, [=](const cpu_core& c) { return c.efficiency_class < max_efficiency_class; });
    u64 allowed_cores_mask = 0;
    for (const cpu_core& c : allowed_cores)
        allowed_cores_mask |= c.logical_cpu_mask;

    inplace_array<l3_cache, 64> allowed_caches;
    for (const l3_cache& l3 : l3_caches)
        if (const u64 mask = l3.logical_cpu_mask & allowed_cores_mask)
            allowed_caches.push_back({mask, l3.size_bytes});
    if (allowed_caches.empty())
        allowed_caches.push_back({allowed_cores_mask, 0});

    u64 picked_caches_mask = 0;
    u32 num_picked_cores = 0;
    while (num_picked_cores < min_pool_cores && !allowed_caches.empty())
    {
        u32 best = 0;
        for (u32 i = 1; i < allowed_caches.size(); i++)
        {
            const l3_cache &a = allowed_caches[i], &b = allowed_caches[best];
            if (a.size_bytes > b.size_bytes || a.size_bytes == b.size_bytes &&
                                               std::countr_zero(a.logical_cpu_mask) < std::countr_zero(b.logical_cpu_mask))
                best = i;
        }
        picked_caches_mask |= allowed_caches[best].logical_cpu_mask;
        allowed_caches.erase(allowed_caches.begin() + best);
        num_picked_cores = 0;
        for (const cpu_core& c : allowed_cores)
            num_picked_cores += (c.logical_cpu_mask & picked_caches_mask) != 0;
    }

    thread_pool_cpus result{};
    auto& masks = result.core_masks;
    for (const cpu_core& c : allowed_cores)
        if (c.logical_cpu_mask & picked_caches_mask)
        {
            const int lowest_bit = std::countr_zero(c.logical_cpu_mask);
            u32 i = masks.size();
            masks.push_back(c.logical_cpu_mask);
            for (; i > 0 && std::countr_zero(masks[i - 1]) > lowest_bit; i--)
                masks[i] = masks[i - 1];
            masks[i] = c.logical_cpu_mask;
            result.logical_cpu_mask |= c.logical_cpu_mask;
        }
    result.is_smt_used = masks.size() < smt_below_cores;
    result.num_workers = result.is_smt_used ? (u32)std::popcount(result.logical_cpu_mask & ~masks[0]) : masks.size() - 1;
    return result;
}

thread_pool_cpus query_thread_pool_cpus() noexcept
{
    const u64 affinity_mask = process_affinity_mask();
    const cpu_topology t = query_cpu_topology(affinity_mask);
    return pick_thread_pool_cpus(t.cores, t.l3_caches, affinity_mask);
}

u64 parse_cpu_list(StringView list) noexcept
{
    const char* p = list.begin();
    const char* const end = list.end();
    const auto skip_whitespace = [&] {
        while (p != end && (*p == ' ' || *p == '\t' || *p == '\n'))
            ++p;
    };
    // Numbers of 64 and above name no bit, so they saturate there.
    const auto read_number = [&] {
        u32 value = 0;
        for (; p != end && *p >= '0' && *p <= '9'; ++p)
            value = value < 64 ? value * 10 + u32(*p - '0') : 64;
        return value;
    };

    u64 mask = 0;
    skip_whitespace();
    if (p == end)
        return 0;
    for (;;)
    {
        const char* const first_digit = p;
        const u32 first = read_number();
        if (p == first_digit)
            return 0;
        u32 last = first;
        if (p != end && *p == '-')
        {
            const char* const last_digit = ++p;
            last = read_number();
            if (p == last_digit || last < first)
                return 0;
        }
        for (u32 cpu = first; cpu <= last && cpu < 64; cpu++)
            mask |= u64{1} << cpu;
        skip_whitespace();
        if (p == end)
            return mask;
        else if (*p != ',')
            return 0;
        else
        {
            ++p;
            skip_whitespace();
        }
    }
}

#ifdef _WIN32

u64 process_affinity_mask() noexcept
{
    DWORD_PTR process_mask, system_mask;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) && process_mask)
        return process_mask;
    else
        return hardware_concurrency_mask();
}

bool set_current_thread_affinity(u64 logical_cpu_mask) noexcept
{
    return SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)logical_cpu_mask) != 0;
}

#elif defined __linux__

u64 process_affinity_mask() noexcept
{
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof set, &set) != 0)
        return hardware_concurrency_mask();
    else
    {
        u64 mask = 0;
        for (u32 i = 0; i < 64; i++)
            if (CPU_ISSET(i, &set))
                mask |= u64{1} << i;
        return mask;
    }
}

bool set_current_thread_affinity(u64 logical_cpu_mask) noexcept
{
    cpu_set_t set;
    CPU_ZERO(&set);
    for (u32 i = 0; i < 64; i++)
        if (logical_cpu_mask >> i & 1)
            CPU_SET(i, &set);
    return sched_setaffinity(0, sizeof set, &set) == 0;
}

#else

u64 process_affinity_mask() noexcept { return hardware_concurrency_mask(); }
bool set_current_thread_affinity(u64) noexcept { return false; }

#endif

} // namespace floormat
