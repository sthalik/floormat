#include "slab-allocator-common.hpp"
#include "compat/slab-allocator.inl"
#include "compat/slab-allocator-std.hpp"
#include "compat/never-destroyed.hpp"
#include "compat/construct-at.hpp"
#include "compat/assert.hpp"
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <nlohmann/json.hpp>

namespace floormat::slab_test {

namespace {

using enum slab::thread_model;

struct tag_containers;
struct tag_json;
struct tag_resource_of_type;
struct tag_json_with_pool_per_type;

constexpr slab::tunables tunables = without_guard_pages(single_threaded);

constinit never_destroyed<slab::memory_resource<tag_containers, tunables>> containers_resource;
constinit never_destroyed<slab::memory_resource<tag_json, tunables>> json_resource;
template<typename U> constinit never_destroyed<slab::memory_resource<tag_resource_of_type, tunables>> resource_of_type;
constinit never_destroyed<slab::memory_resource<tag_json_with_pool_per_type, with_pool_per_type(single_threaded)>>
    json_with_pool_per_type_resource;

struct resource_per_type
{
    template<typename U> static constexpr auto* resource = &resource_of_type<U>.value;
};

template<typename T> using containers_allocator = slab::global_std_allocator<T, &containers_resource.value>;
template<typename T> using json_allocator = slab::global_std_allocator<T, &json_resource.value>;
template<typename T> using per_type_allocator = slab::global_std_allocator<T, resource_per_type{}>;
template<typename T>
using json_with_pool_per_type_allocator = slab::global_std_allocator<T, &json_with_pool_per_type_resource.value>;

using std_json = nlohmann::json;
using slab_json = nlohmann::basic_json<std::map, std::vector, std::string, bool, std_json::number_integer_t,
                                       std_json::number_unsigned_t, std_json::number_float_t, json_allocator>;
using slab_json_with_pool_per_type =
    nlohmann::basic_json<std::map, std::vector, std::string, bool, std_json::number_integer_t,
                         std_json::number_unsigned_t, std_json::number_float_t, json_with_pool_per_type_allocator>;

struct destructor_counter
{
    u32* count;

    explicit constexpr destructor_counter(u32* count) noexcept: count{count} {}
    constexpr ~destructor_counter() noexcept { ++*count; }
};

template<typename R>
bool is_in_arena(const R& r, const void* p)
{
    const slab::detail::arena& arena = r.arena();
    return arena.base && (uptr)p - (uptr)arena.base < (usize)arena.bump_offset * 16;
}

template<typename R>
void destroy_and_recreate(never_destroyed<R>& r)
{
    destroy_at(&r.value);
    construct_at(&r.value);
}

template<typename T, typename R>
u32 num_allocated_objects(R& r)
{
    return r.template num_allocated_buffers<sizeof(T), alignof(T), T>();
}

void test_never_destroyed()
{
    u32 count = 0;
    {
        never_destroyed<destructor_counter> x{&count};
        fm_assert(x.value.count == &count);
    }
    fm_assert(count == 0);
}

void test_default_constructed_containers()
{
    using u64_allocator = containers_allocator<u64>;
    static_assert(std::is_same_v<std::allocator_traits<u64_allocator>::rebind_alloc<u32>, containers_allocator<u32>>);
    static_assert(std::allocator_traits<u64_allocator>::is_always_equal::value);
    static_assert(std::is_nothrow_default_constructible_v<u64_allocator>);
    static_assert(sizeof(std::vector<u64, u64_allocator>) == sizeof(std::vector<u64>));

    const auto& r = containers_resource.value;
    {
        std::vector<u64, u64_allocator> v;
        for (u64 i = 0; i < 1000; i++)
            v.push_back(i * 7);
        std::map<u32, u64, std::less<u32>, containers_allocator<std::pair<const u32, u64>>> m;
        for (u32 i = 0; i < 300; i++)
            m.emplace(i, u64{i} * 3);
        std::basic_string<char, std::char_traits<char>, containers_allocator<char>> s(1000, 'x');
        s += "tail";
        fm_assert(is_in_arena(r, v.data()) && is_in_arena(r, &*m.begin()) && is_in_arena(r, s.data()));
        for (u64 i = 0; i < 1000; i++)
            fm_assert(v[i] == i * 7);
        for (u32 i = 0; i < 300; i++)
            fm_assert(m.at(i) == u64{i} * 3);
        fm_assert(s.size() == 1004 && s[999] == 'x' && s.ends_with("tail"));
    }
    destroy_and_recreate(containers_resource);
}

void test_rebound_types_sharing_resource()
{
    auto& r = containers_resource.value;
    containers_allocator<test_object<48>> a;
    std::allocator_traits<containers_allocator<test_object<48>>>::rebind_alloc<u64> b{a};
    fm_assert(a == b);
    test_object<48>* const p = a.allocate(1);
    u64* const q = b.allocate(1);
    fm_assert(num_allocated_objects<test_object<48>>(r) == 1 && num_allocated_objects<u64>(r) == 1);
    a.deallocate(p, 1);
    b.deallocate(q, 1);
    destroy_and_recreate(containers_resource);
}

void test_basic_json_on_global_resource()
{
    constexpr const char* text = R"({"array":[1,2.5,{"key":"value"}],"name":"slab"})";
    using object_t = slab_json::object_t;
    using array_t = slab_json::array_t;
    const auto& r = json_resource.value;
    {
        const slab_json parsed = slab_json::parse(text);
        slab_json copy = parsed;
        copy["count"] = 3;
        fm_assert(parsed.dump() == text);
        fm_assert(copy.dump() == R"({"array":[1,2.5,{"key":"value"}],"count":3,"name":"slab"})");
        for (const slab_json* j : {&parsed, (const slab_json*)&copy})
        {
            const object_t& object = j->get_ref<const object_t&>();
            const array_t& array = j->at("array").get_ref<const array_t&>();
            fm_assert(is_in_arena(r, &object) && is_in_arena(r, &*object.begin()));
            fm_assert(is_in_arena(r, &array) && is_in_arena(r, array.data()));
        }
        fm_assert(&parsed.get_ref<const object_t&>() != &copy.get_ref<const object_t&>());
    }
    destroy_and_recreate(json_resource);
}

void test_resource_per_type_through_rebind()
{
    using u64_allocator = per_type_allocator<u64>;
    using u32_allocator = std::allocator_traits<u64_allocator>::rebind_alloc<u32>;
    static_assert(std::is_same_v<u32_allocator, per_type_allocator<u32>>);
    const auto& r64 = resource_of_type<u64>.value;
    const auto& r32 = resource_of_type<u32>.value;
    u64_allocator a;
    u32_allocator b{a};
    u64* const p = a.allocate(1);
    u32* const q = b.allocate(1);
    fm_assert(is_in_arena(r64, p) && !is_in_arena(r64, q) && is_in_arena(r32, q));
    b.deallocate(q, 1);
    a.deallocate(p, 1);
    {
        std::map<int, int, std::less<int>, per_type_allocator<std::pair<const int, int>>> m;
        for (int i = 0; i < 307; i++)
            m.emplace(i, i * 7);
        fm_assert(m.size() == 307 && m.at(293) == 2051);
        fm_assert(!resource_of_type<std::pair<const int, int>>.value.arena().base);
    }
    destroy_and_recreate(resource_of_type<u64>);
    destroy_and_recreate(resource_of_type<u32>);
}

void test_basic_json_with_pool_per_type()
{
    constexpr const char* text = R"({"a":[1,"x",{"b":"y"}],"c":"z"})";
    using object_t = slab_json_with_pool_per_type::object_t;
    using array_t = slab_json_with_pool_per_type::array_t;
    using string_t = slab_json_with_pool_per_type::string_t;
    auto& r = json_with_pool_per_type_resource.value;
    {
        const slab_json_with_pool_per_type parsed = slab_json_with_pool_per_type::parse(text);
        fm_assert(num_allocated_objects<object_t>(r) == 2 && num_allocated_objects<array_t>(r) == 1);
        fm_assert(num_allocated_objects<string_t>(r) == 3);
        const slab_json_with_pool_per_type copy = parsed;
        fm_assert(num_allocated_objects<object_t>(r) == 4 && num_allocated_objects<array_t>(r) == 2);
        fm_assert(num_allocated_objects<string_t>(r) == 6);
        fm_assert(parsed.dump() == text && copy.dump() == text);
    }
    fm_assert(num_allocated_objects<object_t>(r) == 0 && num_allocated_objects<array_t>(r) == 0);
    fm_assert(num_allocated_objects<string_t>(r) == 0);
    destroy_and_recreate(json_with_pool_per_type_resource);
}

} // namespace

void test_slab_global_std_allocator()
{
    test_never_destroyed();
    test_default_constructed_containers();
    test_rebound_types_sharing_resource();
    test_basic_json_on_global_resource();
    test_resource_per_type_through_rebind();
    test_basic_json_with_pool_per_type();
}

} // namespace floormat::slab_test
