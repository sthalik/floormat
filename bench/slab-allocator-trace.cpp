#include "compat/slab-allocator.inl"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/format.inl"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/critter.hpp"
#include "src/light.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/anim.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "loader/policy.hpp"
#include "loader/ground-cell.hpp"
#include "loader/wall-cell.hpp"
#include "loader/scenery-cell.hpp"
#include "serialize/json-helper.hpp"
#include "serialize/anim.hpp"
#include "serialize/json-wrapper.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <cr/Array.h>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/Path.h>
#include <cr/String.h>
#include <cr/StringView.h>
#include <benchmark/benchmark.h>
#ifdef __linux__
#include <dlfcn.h>
#endif

namespace floormat {

namespace {

struct trace_record { u64 ptr, old_ptr, bytes; u32 align, kind; };
static_assert(sizeof(trace_record) == 32);

enum : u32 { kind_new, kind_delete, kind_malloc, kind_calloc, kind_realloc, kind_free };

enum class replay_op : u8 { new_object, delete_object, malloc_bytes, calloc_bytes, realloc_bytes, free_bytes };

struct replay_event
{
    u32 bytes, old_bytes, slot, old_slot;
    replay_op op;
    u8 align_log2;
};

struct replay_trace
{
    Array<replay_event> events;
    u32 num_slots = 0, num_allocations = 0, num_reallocations = 0, num_unmatched_frees = 0, num_freed_at_end = 0;
    u64 max_live_bytes = 0;
};

struct live_buffer
{
    u32 slot, bytes;
    u8 align_log2;
    bool is_object;
};

u8 align_log2_of(u64 align)
{
    return align > 16 ? (u8)std::countr_zero(align) : 4;
}

replay_trace convert_trace(ArrayView<const trace_record> records)
{
    replay_trace t;
    arrayReserve(t.events, records.size() + 1024);
    std::unordered_map<u64, live_buffer> live;
    Array<u32> free_slots;
    arrayReserve(free_slots, 1024);
    u64 live_bytes = 0;

    const auto take_slot = [&] {
        if (free_slots.isEmpty())
            return t.num_slots++;
        else
        {
            const u32 slot = free_slots.back();
            arrayRemoveSuffix(free_slots);
            return slot;
        }
    };
    const auto add_buffer = [&](u64 ptr, u64 bytes, u64 align, replay_op op) {
        fm_assert(bytes <= (u32)-1);
        fm_assert(align <= 8192);
        const u8 align_log2 = align_log2_of(align);
        if (op == replay_op::malloc_bytes && align_log2 > 4)
            op = replay_op::new_object;
        const u32 slot = take_slot();
        live.insert_or_assign(ptr, live_buffer{slot, (u32)bytes, align_log2, op == replay_op::new_object});
        arrayAppend(t.events, replay_event{(u32)bytes, 0, slot, 0, op, align_log2});
        t.num_allocations++;
        live_bytes += bytes;
        t.max_live_bytes = std::max(t.max_live_bytes, live_bytes);
    };
    const auto remove_buffer = [&](const live_buffer& b) {
        const replay_op op = b.is_object ? replay_op::delete_object : replay_op::free_bytes;
        arrayAppend(t.events, replay_event{b.bytes, 0, b.slot, 0, op, b.align_log2});
        arrayAppend(free_slots, b.slot);
        live_bytes -= b.bytes;
    };
    const auto free_buffer = [&](u64 ptr) {
        if (!ptr)
            return;
        const auto it = live.find(ptr);
        if (it == live.end())
            t.num_unmatched_frees++;
        else
        {
            remove_buffer(it->second);
            live.erase(it);
        }
    };

    for (const trace_record& r : records)
        switch (r.kind)
        {
        case kind_new:
            if (r.ptr)
                add_buffer(r.ptr, r.bytes, r.align, replay_op::new_object);
            break;
        case kind_malloc:
            if (r.ptr)
                add_buffer(r.ptr, r.bytes, r.align, replay_op::malloc_bytes);
            break;
        case kind_calloc:
            if (r.ptr)
                add_buffer(r.ptr, r.bytes, 0, replay_op::calloc_bytes);
            break;
        case kind_delete:
        case kind_free:
            free_buffer(r.ptr);
            break;
        case kind_realloc: {
            const auto it = r.old_ptr ? live.find(r.old_ptr) : live.end();
            if (!r.ptr)
            {
                if (r.old_ptr && !r.bytes)
                    free_buffer(r.old_ptr);
            }
            else if (it == live.end())
            {
                t.num_unmatched_frees += r.old_ptr != 0;
                add_buffer(r.ptr, r.bytes, 0, replay_op::malloc_bytes);
            }
            else if (it->second.is_object || it->second.align_log2 > 4)
            {
                const live_buffer old = it->second;
                live.erase(it);
                remove_buffer(old);
                add_buffer(r.ptr, r.bytes, 0, replay_op::malloc_bytes);
            }
            else
            {
                fm_assert(r.bytes <= (u32)-1);
                const live_buffer old = it->second;
                live.erase(it);
                const u32 slot = take_slot();
                arrayAppend(t.events, replay_event{(u32)r.bytes, old.bytes, slot, old.slot, replay_op::realloc_bytes, 4});
                arrayAppend(free_slots, old.slot);
                live.insert_or_assign(r.ptr, live_buffer{slot, (u32)r.bytes, 4, false});
                live_bytes += r.bytes - old.bytes;
                t.max_live_bytes = std::max(t.max_live_bytes, live_bytes);
                t.num_reallocations++;
            }
            break;
        }
        default:
            fm_abort("bad trace record kind %u", r.kind);
        }

    Array<live_buffer> left;
    arrayReserve(left, live.size());
    for (const auto& [ptr, b] : live)
        arrayAppend(left, b);
    std::sort(left.begin(), left.end(), [](const live_buffer& a, const live_buffer& b) { return a.slot < b.slot; });
    for (const live_buffer& b : left)
        remove_buffer(b);
    t.num_freed_at_end = (u32)left.size();
    return t;
}

Optional<replay_trace> read_replay_trace(const char* workload)
{
    const char* const dir = std::getenv("FLOORMAT_ALLOC_TRACE_DIR");
    if (!dir || !*dir)
        return {};
    const Optional<Array<char>> data = Path::read(Path::join(StringView{dir}, StringView{workload} + ".trace"_s));
    if (!data || data->size() % sizeof(trace_record))
        return {};
    Array<trace_record> records{NoInit, data->size() / sizeof(trace_record)};
    std::memcpy(records.data(), data->data(), data->size());
    return convert_trace(records);
}

struct system_replayer
{
    void* allocate(const replay_event& e);
    void* reallocate(void* p, const replay_event& e);
    void deallocate(void* p, const replay_event& e) noexcept;
};

void* system_replayer::allocate(const replay_event& e)
{
    void* p = nullptr;
    switch (e.op)
    {
    case replay_op::new_object:
        p = e.align_log2 > 4 ? ::operator new(e.bytes, std::align_val_t{usize{1} << e.align_log2})
                             : ::operator new(e.bytes);
        break;
    case replay_op::calloc_bytes:
        return std::calloc(1, e.bytes);
    case replay_op::malloc_bytes:
        p = std::malloc(e.bytes);
        break;
    case replay_op::realloc_bytes:
    case replay_op::delete_object:
    case replay_op::free_bytes:
        fm_assert(false);
    }
    if (e.bytes)
        *static_cast<char*>(p) = (char)e.slot;
    return p;
}

void* system_replayer::reallocate(void* p, const replay_event& e)
{
    return std::realloc(p, e.bytes);
}

void system_replayer::deallocate(void* p, const replay_event& e) noexcept
{
    if (e.op == replay_op::free_bytes)
        std::free(p);
    else if (e.align_log2 > 4)
#ifdef __cpp_sized_deallocation
        ::operator delete(p, e.bytes, std::align_val_t{usize{1} << e.align_log2});
#else
        ::operator delete(p, std::align_val_t{usize{1} << e.align_log2});
#endif
    else
#ifdef __cpp_sized_deallocation
        ::operator delete(p, e.bytes);
#else
        ::operator delete(p);
#endif
}

template<typename Resource>
void* allocate_aligned_array(Resource& r, usize bytes, u8 align_log2)
{
    switch (align_log2)
    {
    case 5: return r.template allocate_array<32>(bytes);
    case 6: return r.template allocate_array<64>(bytes);
    case 7: return r.template allocate_array<128>(bytes);
    case 8: return r.template allocate_array<256>(bytes);
    case 9: return r.template allocate_array<512>(bytes);
    case 10: return r.template allocate_array<1024>(bytes);
    case 11: return r.template allocate_array<2048>(bytes);
    case 12: return r.template allocate_array<4096>(bytes);
    case 13: return r.template allocate_array<8192>(bytes);
    default:
        fm_assert(align_log2 == 4);
        return r.template allocate_array<16>(bytes);
    }
}

template<typename Resource>
void deallocate_aligned_array(Resource& r, void* p, usize bytes, u8 align_log2) noexcept
{
    switch (align_log2)
    {
    case 5: r.template deallocate_array<32>(p, bytes); break;
    case 6: r.template deallocate_array<64>(p, bytes); break;
    case 7: r.template deallocate_array<128>(p, bytes); break;
    case 8: r.template deallocate_array<256>(p, bytes); break;
    case 9: r.template deallocate_array<512>(p, bytes); break;
    case 10: r.template deallocate_array<1024>(p, bytes); break;
    case 11: r.template deallocate_array<2048>(p, bytes); break;
    case 12: r.template deallocate_array<4096>(p, bytes); break;
    case 13: r.template deallocate_array<8192>(p, bytes); break;
    default:
        fm_assert(align_log2 == 4);
        r.template deallocate_array<16>(p, bytes);
        break;
    }
}

template<typename Resource>
struct slab_replayer
{
    Resource r;

    void* allocate(const replay_event& e);
    void* reallocate(void* p, const replay_event& e);
    void deallocate(void* p, const replay_event& e) noexcept;
};

template<typename Resource>
void* slab_replayer<Resource>::allocate(const replay_event& e)
{
    void* const p = allocate_aligned_array(r, e.bytes, e.align_log2);
    if (e.op == replay_op::calloc_bytes)
        std::memset(p, 0, e.bytes);
    else if (e.bytes)
        *static_cast<char*>(p) = (char)e.slot;
    return p;
}

template<typename Resource>
void* slab_replayer<Resource>::reallocate(void* p, const replay_event& e)
{
    void* const q = r.template allocate_array<16>(e.bytes);
    std::memcpy(q, p, std::min(e.bytes, e.old_bytes));
    r.template deallocate_array<16>(p, e.old_bytes);
    return q;
}

template<typename Resource>
void slab_replayer<Resource>::deallocate(void* p, const replay_event& e) noexcept
{
    deallocate_aligned_array(r, p, e.bytes, e.align_log2);
}

template<typename Replayer>
void replay_events(Replayer& h, const replay_trace& t, void** slots)
{
    for (const replay_event& e : t.events)
        switch (e.op)
        {
        case replay_op::new_object:
        case replay_op::malloc_bytes:
        case replay_op::calloc_bytes:
            slots[e.slot] = h.allocate(e);
            break;
        case replay_op::realloc_bytes:
            slots[e.slot] = h.reallocate(slots[e.old_slot], e);
            break;
        case replay_op::delete_object:
        case replay_op::free_bytes:
            h.deallocate(slots[e.slot], e);
            break;
        }
}

template<typename Replayer>
void Slab_ReplayTrace(benchmark::State& state, const char* workload)
{
    const Optional<replay_trace> t = read_replay_trace(workload);
    if (!t)
        state.SkipWithError("no trace in FLOORMAT_ALLOC_TRACE_DIR");
    else
    {
        Array<void*> slots{ValueInit, t->num_slots};
        Replayer h;
        for (auto _ : state)
        {
            replay_events(h, *t, slots.data());
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations() * (i64)t->events.size());
        state.counters["events"] = (double)t->events.size();
        state.counters["allocations"] = t->num_allocations;
        state.counters["reallocations"] = t->num_reallocations;
        state.counters["max_live_kib"] = (double)t->max_live_bytes / 1024;
        state.counters["unmatched_frees"] = t->num_unmatched_frees;
        state.counters["freed_at_end"] = t->num_freed_at_end;
    }
}

struct trace_tag;

constexpr slab::tunables trace_tunables(slab::thread_model thread_model)
{
    return {.thread_model = thread_model, .buffer_source = slab::buffer_source::slabs};
}

template<slab::thread_model ThreadModel>
using trace_resource = slab::memory_resource<trace_tag, trace_tunables(ThreadModel)>;

constexpr const char* workload_names[] = {
    "world_load_proto_31", "world_load_proto_32", "world_load_generated",
    "world_save_proto_32", "world_save_generated", "json_atlas_defs",
};

#ifdef __linux__
String path_in_temp_dir(StringView name)
{
    return Path::join(loader.TEMP_PATH, name);
}

void make_generated_world(world& w, i16 side)
{
    const auto& tiles = loader.ground_atlas("tiles");
    const auto& wall = loader.wall_atlas("empty", loader_policy::warn);
    const scenery_proto& table = loader.scenery("table1");
    const scenery_proto& door = loader.scenery("door1");
    const scenery_proto& control_panel = loader.scenery("control panel (wall) 1");
    u32 num_critters = 0;
    for (i16 y = 0; y < side; y++)
        for (i16 x = 0; x < side; x++)
        {
            const auto ch = chunk_coords_{x, y, 0};
            chunk& c = w[ch];
            c.mark_modified();
            for (u32 j = 0; j < TILE_MAX_DIM; j++)
                for (u32 i = 0; i < TILE_MAX_DIM; i++)
                {
                    c[{i, j}].ground() = {tiles, variant_t((j * TILE_MAX_DIM + i) % tiles->num_tiles())};
                    if (j % 4 == 0 && i % 3 != 0)
                        c[{i, j}].wall_north() = {wall, 0};
                    if (i % 5 == 0 && j % 2 == 1)
                        c[{i, j}].wall_west() = {wall, 0};
                    const auto pos = global_coords{ch, {i, j}};
                    switch ((i * 7 + j * 13 + (u32)x * 3 + (u32)y * 5) % 16)
                    {
                    case 0:
                        w.make_scenery(w.make_id(), pos, scenery_proto(table));
                        break;
                    case 5:
                        w.make_scenery(w.make_id(), pos, scenery_proto(door));
                        break;
                    case 9:
                        w.make_scenery(w.make_id(), pos, scenery_proto(control_panel));
                        break;
                    case 11: {
                        auto L = light_proto{};
                        L.color = {64, 128, 252, 201};
                        L.max_distance = float{0.125};
                        L.falloff = light_falloff::quadratic;
                        w.make_object<light>(w.make_id(), pos, L);
                        break;
                    }
                    case 14: {
                        char buf[32];
                        const auto len = snformat(buf, "npc {}", num_critters++);
                        critter_proto cproto;
                        cproto.name = StringView{buf, len};
                        w.make_object<critter>(w.make_id(), pos, cproto);
                        break;
                    }
                    default:
                        break;
                    }
                }
        }
}

auto make_world_load_workload(String path)
{
    (void)world::deserialize(path, loader_policy::warn);
    return [path = std::move(path)] {
        auto w = world::deserialize(path, loader_policy::warn);
        benchmark::DoNotOptimize(&w);
    };
}

auto make_world_save_workload(const String& from)
{
    auto w = world::deserialize(from, loader_policy::warn);
    String to = path_in_temp_dir("slab-trace-save.dat"_s);
    w.serialize(to);
    return [w = std::move(w), to = std::move(to)]() mutable { w.serialize(to); };
}

String generated_save_path()
{
    String path = path_in_temp_dir("slab-trace-generated.dat"_s);
    auto w = world();
    make_generated_world(w, 16);
    w.serialize(path);
    return path;
}

Array<String> json_paths_in(StringView dir, StringView excluded_name)
{
    const auto names = Path::list(dir, Path::ListFlag::SkipDirectories | Path::ListFlag::SkipDotAndDotDot |
                                           Path::ListFlag::SortAscending);
    fm_assert(names);
    Array<String> paths;
    arrayReserve(paths, names->size());
    for (const String& name : *names)
        if (name.hasSuffix(".json"_s) && name != excluded_name)
            arrayAppend(paths, Path::join(dir, name));
    return paths;
}

auto make_json_atlas_defs_workload()
{
    Array<String> anim_paths = json_paths_in("anim"_s, {});
    for (String& path : json_paths_in("scenery"_s, "scenery.json"_s))
        arrayAppend(anim_paths, std::move(path));
    for (String& path : json_paths_in("scenery/furniture"_s, {}))
        arrayAppend(anim_paths, std::move(path));
    Array<String> wall_paths = json_paths_in("walls"_s, "walls.json"_s);
    auto run = [anim_paths = std::move(anim_paths), wall_paths = std::move(wall_paths)] {
        for (const String& path : anim_paths)
        {
            auto def = json_helper::from_json<anim_def>(path);
            benchmark::DoNotOptimize(&def);
        }
        for (const String& path : wall_paths)
        {
            auto def = wall_atlas_def::deserialize(path);
            benchmark::DoNotOptimize(&def);
        }
        auto ground = ground_cell::load_atlases_from_json();
        auto walls = wall_cell::load_atlases_from_json();
        auto sceneries = scenery_cell::load_atlases_from_json();
        benchmark::DoNotOptimize(&ground);
        benchmark::DoNotOptimize(&walls);
        benchmark::DoNotOptimize(&sceneries);
    };
    run();
    return run;
}

using trace_start_fn = void (*)(const char*);
using trace_stop_fn = void (*)();

template<typename MakeWorkload>
void Slab_RecordTrace(benchmark::State& state, const char* workload, MakeWorkload make_workload)
{
    const char* const dir = std::getenv("FLOORMAT_ALLOC_TRACE_DIR");
    const auto start = reinterpret_cast<trace_start_fn>(dlsym(RTLD_DEFAULT, "fm_alloc_trace_start"));
    const auto stop = reinterpret_cast<trace_stop_fn>(dlsym(RTLD_DEFAULT, "fm_alloc_trace_stop"));
    if (!dir || !*dir || !start || !stop)
        state.SkipWithError("needs FLOORMAT_ALLOC_TRACE_DIR and LD_PRELOAD of alloc-trace-shim.so, built with "
                            "clang++ -O2 -std=c++20 -shared -fPIC -o alloc-trace-shim.so alloc-trace-shim.cpp");
    else
    {
        const String path = Path::join(StringView{dir}, StringView{workload} + ".trace"_s);
        auto run = make_workload();
        for (auto _ : state)
        {
            start(path.data());
            run();
            stop();
        }
    }
}

template<typename MakeWorkload>
void register_record_case(const char* workload, MakeWorkload make_workload)
{
    benchmark::RegisterBenchmark(std::string{"Slab_RecordTrace/"} + workload,
                                 [=](benchmark::State& state) { Slab_RecordTrace(state, workload, make_workload); })
        ->Iterations(1)->Unit(benchmark::kMillisecond);
}
#endif

[[maybe_unused]] const int registered = [] {
#ifdef __linux__
    register_record_case("world_load_proto_31", [] {
        return make_world_load_workload(path_in_temp_dir("bench/save/proto-31-quicksave.dat"_s));
    });
    register_record_case("world_load_proto_32", [] {
        return make_world_load_workload(path_in_temp_dir("bench/save/proto-32-quicksave.dat"_s));
    });
    register_record_case("world_load_generated", [] { return make_world_load_workload(generated_save_path()); });
    register_record_case("world_save_proto_32", [] {
        return make_world_save_workload(path_in_temp_dir("bench/save/proto-32-quicksave.dat"_s));
    });
    register_record_case("world_save_generated", [] { return make_world_save_workload(generated_save_path()); });
    register_record_case("json_atlas_defs", [] { return make_json_atlas_defs_workload(); });
#endif
    using enum slab::thread_model;
    for (const char* workload : workload_names)
    {
        const std::string suffix = std::string{"/"} + workload;
        benchmark::RegisterBenchmark("Slab_ReplayTrace/system" + suffix, &Slab_ReplayTrace<system_replayer>, workload)
            ->Unit(benchmark::kMicrosecond);
        benchmark::RegisterBenchmark("Slab_ReplayTrace/single_threaded" + suffix,
                                     &Slab_ReplayTrace<slab_replayer<trace_resource<single_threaded>>>, workload)
            ->Unit(benchmark::kMicrosecond);
        benchmark::RegisterBenchmark("Slab_ReplayTrace/single_allocating_thread" + suffix,
                                     &Slab_ReplayTrace<slab_replayer<trace_resource<single_allocating_thread>>>,
                                     workload)
            ->Unit(benchmark::kMicrosecond);
    }
    return 0;
}();

} // namespace

} // namespace floormat
