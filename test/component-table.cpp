#include "app.hpp"
#include "entity/component.hpp"
#include "entity/name-of.hpp"
#include "src/component-table.inl"
#include "random/random.hpp"
#include <type_traits>
#include <utility>
#include <vector>
#include <cr/Format.h>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>
#include <cr/String.h>
#include <cr/StringView.h>

namespace floormat::component_table_test {

struct Stats;
struct Label;
struct Marked;
struct Velocity;

enum class stats_field : u8 { level, hit_points, experience, COUNT };
enum class label_field : u8 { text, tracked, COUNT };
enum class velocity_field : u8 { dx, dy, COUNT };

struct counted
{
    static inline i32 num_live = 0;
    u64 value = 0;

    counted() noexcept { num_live++; }
    explicit counted(u64 value) noexcept : value{value} { num_live++; }
    counted(counted&& other) noexcept : value{other.value} { num_live++; }
    counted& operator=(counted&&) noexcept = default;
    ~counted() noexcept { num_live--; }
};

} // namespace floormat::component_table_test

namespace floormat {

template<> struct component_fields<component_table_test::Stats>
{
    static consteval auto fields()
    {
        using enum component_table_test::stats_field;
        return field_list<COUNT,
            component_field<level, u8>,
            component_field<hit_points, u32>,
            component_field<experience, u64>>{};
    }
};

template<> struct component_fields<component_table_test::Label>
{
    static consteval auto fields()
    {
        using enum component_table_test::label_field;
        return field_list<COUNT,
            component_field<text, String>,
            component_field<tracked, component_table_test::counted>>{};
    }
};

template<> struct component_fields<component_table_test::Marked>
{
    static consteval auto fields()
    {
        return no_fields{};
    }
};

template<> struct component_fields<component_table_test::Velocity>
{
    static consteval auto fields()
    {
        using enum component_table_test::velocity_field;
        return field_list<COUNT,
            component_field<dx, i16>,
            component_field<dy, float>>{};
    }
};

} // namespace floormat

namespace floormat::component_table_test {

using table = component_table<Stats, Label, Marked>;

using level_field = component_field<stats_field::level, u8>;
using hit_points_field = component_field<stats_field::hit_points, u32>;
using experience_field = component_field<stats_field::experience, u64>;

static_assert(is_field_list_in_enumerator_order<stats_field::COUNT, level_field, hit_points_field, experience_field>);
static_assert(is_field_list_in_enumerator_order<label_field::COUNT, component_field<label_field::text, String>,
                                                component_field<label_field::tracked, counted>>);
static_assert(!is_field_list_in_enumerator_order<stats_field::COUNT, level_field, hit_points_field>);
static_assert(!is_field_list_in_enumerator_order<stats_field::COUNT, level_field, experience_field, hit_points_field>);
static_assert(!is_field_list_in_enumerator_order<stats_field::COUNT, level_field,
                                                 component_field<label_field::tracked, u32>, experience_field>);
static_assert(!is_field_list_in_enumerator_order<stats_field::COUNT, level_field, hit_points_field, experience_field,
                                                 component_field<stats_field::COUNT, u8>>);

template<typename Component>
concept has_field_enum = requires { typename field_enum_of<Component>; };

static_assert(std::is_same_v<field_enum_of<Stats>, stats_field>);
static_assert(std::is_same_v<field_enum_of<Velocity>, velocity_field>);
static_assert(!has_field_enum<Marked>);
static_assert(field_count<Stats> == 3);
static_assert(field_count<Label> == 2);
static_assert(field_count<Velocity> == 2);
static_assert(field_count<Marked> == 0);

static_assert(field_index<stats_field::level, Stats, Label, Velocity> == 0);
static_assert(field_index<stats_field::experience, Stats, Label, Velocity> == 2);
static_assert(field_index<label_field::text, Stats, Label, Velocity> == 3);
static_assert(field_index<label_field::tracked, Stats, Label, Velocity> == 4);
static_assert(field_index<velocity_field::dx, Stats, Label, Velocity> == 5);
static_assert(field_index<velocity_field::dy, Stats, Label, Velocity> == 6);
static_assert(field_index<velocity_field::dy, Velocity, Stats, Label> == 1);
static_assert(field_index<label_field::text, Stats, Marked, Label> == 3);

static_assert(std::is_same_v<field_type<stats_field::level, Stats, Label, Velocity>, u8>);
static_assert(std::is_same_v<field_type<stats_field::hit_points, Stats, Label, Velocity>, u32>);
static_assert(std::is_same_v<field_type<stats_field::experience, Stats, Label, Velocity>, u64>);
static_assert(std::is_same_v<field_type<label_field::text, Stats, Label, Velocity>, String>);
static_assert(std::is_same_v<field_type<label_field::tracked, Stats, Label, Velocity>, counted>);
static_assert(std::is_same_v<field_type<velocity_field::dx, Stats, Label, Velocity>, i16>);
static_assert(std::is_same_v<field_type<velocity_field::dy, Stats, Label, Velocity>, float>);

static_assert(std::is_same_v<decltype(std::declval<table&>().get<stats_field::hit_points>(0)), u32&>);
static_assert(std::is_same_v<decltype(std::declval<const table&>().get<stats_field::hit_points>(0)), const u32&>);
static_assert(std::is_same_v<decltype(std::declval<table&>().field_array<label_field::text>()), ArrayView<String>>);
static_assert(std::is_same_v<decltype(std::declval<const table&>().field_array<label_field::text>()),
                             ArrayView<const String>>);
static_assert(std::is_same_v<decltype(std::declval<table&>()[0].get<label_field::tracked>()), counted&>);
static_assert(std::is_same_v<decltype(std::declval<const table&>()[0].get<label_field::tracked>()), const counted&>);
static_assert(!std::is_copy_constructible_v<table> && !std::is_copy_assignable_v<table>);
static_assert(std::is_nothrow_move_constructible_v<table> && std::is_nothrow_move_assignable_v<table>);

static_assert(enumerator_name<label_field::tracked> == "tracked"_s);

namespace {

constexpr u32 num_ids = 200;

object_id id_at(u32 k) { return 101 + 3 * object_id{k}; }

u8 level_of(object_id id) { return (u8)(id * 7 + 3); }
u32 hit_points_of(object_id id) { return (u32)(id * 2654435761u); }
u64 experience_of(object_id id) { return id * 0x9e3779b97f4a7c15u + 11; }
String text_of(object_id id) { return Utility::format("text of object {}, longer than the small string buffer", id); }
u64 tracked_value_of(object_id id) { return id ^ 0x5bd1e995u; }

Array<object_id> shuffled_ids(u64 seed)
{
    Array<object_id> ids{NoInit, num_ids};
    for (u32 k = 0; k < num_ids; k++)
        ids[k] = id_at(k);
    Random::xoshiro256starstar rng;
    Random::seed(rng, seed);
    for (u32 i = num_ids; i > 1; i--)
        std::swap(ids[i - 1], ids[Random::next(rng) % i]);
    return ids;
}

void write_row(table& t, u32 row)
{
    const object_id id = t.ids()[row];
    t.get<stats_field::level>(row) = level_of(id);
    t.get<stats_field::hit_points>(row) = hit_points_of(id);
    t.get<stats_field::experience>(row) = experience_of(id);
    t.get<label_field::text>(row) = text_of(id);
    t.get<label_field::tracked>(row).value = tracked_value_of(id);
}

void check_row(const table& t, u32 row)
{
    const object_id id = t.ids()[row];
    fm_assert(t.get<stats_field::level>(row) == level_of(id));
    fm_assert(t.get<stats_field::hit_points>(row) == hit_points_of(id));
    fm_assert(t.get<stats_field::experience>(row) == experience_of(id));
    fm_assert(t.get<label_field::text>(row) == text_of(id));
    fm_assert(t.get<label_field::tracked>(row).value == tracked_value_of(id));
}

void check_ascending(const table& t)
{
    const auto ids = t.ids();
    for (u32 i = 1; i < ids.size(); i++)
        fm_assert(ids[i - 1] < ids[i]);
}

void fill(table& t, ArrayView<const object_id> ids)
{
    for (object_id id : ids)
    {
        const u32 row = t.insert_row(id);
        fm_assert(t.ids()[row] == id);
        write_row(t, row);
    }
}

void test_insert_keeps_rows_sorted()
{
    const i32 num_live_before = counted::num_live;
    table t;
    for (object_id id : shuffled_ids(17))
    {
        const u32 row = t.insert_row(id);
        fm_assert(t.ids()[row] == id);
        fm_assert(t.get<stats_field::level>(row) == 0);
        fm_assert(t.get<stats_field::hit_points>(row) == 0);
        fm_assert(t.get<stats_field::experience>(row) == 0);
        fm_assert(t.get<label_field::text>(row).isEmpty());
        fm_assert(t.get<label_field::tracked>(row).value == 0);
        write_row(t, row);
    }
    fm_assert(t.size() == num_ids);
    check_ascending(t);
    for (u32 k = 0; k < num_ids; k++)
    {
        fm_assert(t.ids()[k] == id_at(k));
        check_row(t, k);
    }
    fm_assert(counted::num_live == num_live_before + (i32)num_ids);
}

void test_find_row()
{
    table t;
    fm_assert(!t.find_row(0));
    fm_assert(!t.find_row(id_at(0)));
    fill(t, shuffled_ids(23));
    for (u32 row = 0; row < t.size(); row++)
    {
        const auto found = t.find_row(t.ids()[row]);
        fm_assert(found && *found == row);
    }
    fm_assert(!t.find_row(id_at(0) - 1));
    fm_assert(!t.find_row(id_at(57) + 1));
    fm_assert(!t.find_row(id_at(num_ids - 1) + 1));
}

void test_erase_row()
{
    const i32 num_live_before = counted::num_live;
    table t;
    fill(t, shuffled_ids(29));
    Array<object_id> expected_ids;
    arrayAppend(expected_ids, t.ids());
    const auto erase_and_check = [&](u32 row) {
        t.erase_row(row);
        arrayRemove(expected_ids, row);
        fm_assert(t.size() == expected_ids.size());
        for (u32 i = 0; i < t.size(); i++)
        {
            fm_assert(t.ids()[i] == expected_ids[i]);
            check_row(t, i);
        }
        fm_assert(counted::num_live - num_live_before == (i32)t.size());
    };
    erase_and_check(0);
    erase_and_check(t.size() / 2);
    erase_and_check(t.size() - 1);
}

void test_move_row_to()
{
    table a, b;
    fill(a, shuffled_ids(31));
    const i32 num_live = counted::num_live;
    for (object_id id : shuffled_ids(37))
    {
        const auto row = a.find_row(id);
        fm_assert(row);
        const u32 dst_row = a.move_row_to(b, *row);
        fm_assert(b.ids()[dst_row] == id);
        fm_assert(!a.find_row(id));
        check_row(b, dst_row);
        check_ascending(a);
        check_ascending(b);
        fm_assert(counted::num_live == num_live);
    }
    fm_assert(a.empty());
    fm_assert(b.size() == num_ids);
    for (u32 row = 0; row < b.size(); row++)
        check_row(b, row);
}

template<auto Enumerator>
void check_row_ref_field(table& t, u32 row)
{
    const table& const_t = t;
    fm_assert(&t[row].get<Enumerator>() == &t.field_array<Enumerator>()[row]);
    fm_assert(&const_t[row].get<Enumerator>() == &const_t.field_array<Enumerator>()[row]);
}

void test_row_refs()
{
    table t;
    fill(t, shuffled_ids(41));
    const table& const_t = t;
    for (u32 row = 0; row < t.size(); row++)
    {
        fm_assert(t[row].id() == t.ids()[row]);
        fm_assert(const_t[row].id() == t.ids()[row]);
        check_row_ref_field<stats_field::level>(t, row);
        check_row_ref_field<stats_field::hit_points>(t, row);
        check_row_ref_field<stats_field::experience>(t, row);
        check_row_ref_field<label_field::text>(t, row);
        check_row_ref_field<label_field::tracked>(t, row);
    }
}

void test_lifetime()
{
    const i32 num_live_before = counted::num_live;
    {
        table t;
        fill(t, shuffled_ids(43));
        fm_assert(counted::num_live == num_live_before + (i32)num_ids);
    }
    fm_assert(counted::num_live == num_live_before);
    {
        table a;
        fill(a, shuffled_ids(47));
        {
            table b{move(a)};
            fm_assert(a.empty());
            fm_assert(b.size() == num_ids);
            fm_assert(counted::num_live == num_live_before + (i32)num_ids);
        }
        fm_assert(counted::num_live == num_live_before);
        fill(a, shuffled_ids(53));
        for (u32 row = 0; row < a.size(); row++)
            check_row(a, row);
    }
    fm_assert(counted::num_live == num_live_before);
    {
        const auto ids = shuffled_ids(59);
        table a, b;
        fill(a, ids.prefix(num_ids / 2));
        fill(b, ids.exceptPrefix(num_ids / 2));
        b = move(a);
        fm_assert(b.size() == num_ids / 2);
        for (u32 row = 0; row < b.size(); row++)
            check_row(b, row);
        check_ascending(a);
        for (u32 row = 0; row < a.size(); row++)
            check_row(a, row);
    }
    fm_assert(counted::num_live == num_live_before);
    {
        table t;
        fill(t, shuffled_ids(61));
        t.clear();
        fm_assert(t.empty());
        fm_assert(counted::num_live == num_live_before);
        fill(t, shuffled_ids(67));
        for (u32 row = 0; row < t.size(); row++)
            check_row(t, row);
    }
    fm_assert(counted::num_live == num_live_before);
}

struct model_row
{
    object_id id;
    u8 level;
    u32 hit_points;
    u64 experience;
    String text;
    u64 tracked_value;
};

u32 model_lower_bound(const std::vector<model_row>& model, object_id id)
{
    u32 i = 0;
    while (i < model.size() && model[i].id < id)
        i++;
    return i;
}

void check_against_model(const table& t, const std::vector<model_row>& model)
{
    fm_assert(t.size() == model.size());
    for (u32 row = 0; row < t.size(); row++)
    {
        const model_row& m = model[row];
        fm_assert(t.ids()[row] == m.id);
        fm_assert(t.get<stats_field::level>(row) == m.level);
        fm_assert(t.get<stats_field::hit_points>(row) == m.hit_points);
        fm_assert(t.get<stats_field::experience>(row) == m.experience);
        fm_assert(t.get<label_field::text>(row) == m.text);
        fm_assert(t.get<label_field::tracked>(row).value == m.tracked_value);
    }
}

void test_random_operations()
{
    constexpr u32 num_ops = 20000, num_possible_ids = 2048;
    const i32 num_live_before = counted::num_live;
    Random::xoshiro256starstar rng;
    Random::seed(rng, 71);
    const auto random_below = [&](u32 n) { return (u32)(Random::next(rng) % n); };
    const auto random_text = [&] {
        return Utility::format("random text {}, longer than the small string buffer", Random::next(rng));
    };

    table tables[2];
    std::vector<model_row> models[2];
    bool is_id_used[num_possible_ids] = {};
    u32 num_used_ids = 0;

    for (u32 op = 0; op < num_ops; op++)
    {
        u32 k = random_below(2);
        if (tables[k].empty() && num_used_ids == num_possible_ids)
            k = 1 - k;
        table& t = tables[k];
        std::vector<model_row>& model = models[k];
        u32 kind = random_below(100);
        if (t.empty())
            kind = 0;
        else if (kind < 32 && num_used_ids == num_possible_ids)
            kind = 32;

        if (kind < 32)
        {
            u32 slot = random_below(num_possible_ids);
            while (is_id_used[slot])
                slot = random_below(num_possible_ids);
            is_id_used[slot] = true;
            num_used_ids++;
            model_row m{ id_at(slot), (u8)Random::next(rng), (u32)Random::next(rng), Random::next(rng),
                         random_text(), Random::next(rng) };
            const u32 row = t.insert_row(m.id);
            fm_assert(row == model_lower_bound(model, m.id));
            t.get<stats_field::level>(row) = m.level;
            t.get<stats_field::hit_points>(row) = m.hit_points;
            t.get<stats_field::experience>(row) = m.experience;
            t.get<label_field::text>(row) = m.text;
            t.get<label_field::tracked>(row).value = m.tracked_value;
            model.insert(model.begin() + row, move(m));
        }
        else if (kind < 60)
        {
            const u32 row = random_below(t.size());
            is_id_used[(model[row].id - id_at(0)) / 3] = false;
            num_used_ids--;
            t.erase_row(row);
            model.erase(model.begin() + row);
        }
        else if (kind < 80)
        {
            const u32 row = random_below(t.size());
            model_row m = move(model[row]);
            model.erase(model.begin() + row);
            std::vector<model_row>& dst_model = models[1 - k];
            const u32 dst_row = t.move_row_to(tables[1 - k], row);
            fm_assert(dst_row == model_lower_bound(dst_model, m.id));
            dst_model.insert(dst_model.begin() + dst_row, move(m));
        }
        else
        {
            const u32 row = random_below(t.size());
            model_row& m = model[row];
            switch (random_below(5))
            {
            case 0: t.get<stats_field::level>(row) = m.level = (u8)Random::next(rng); break;
            case 1: t.get<stats_field::hit_points>(row) = m.hit_points = (u32)Random::next(rng); break;
            case 2: t.get<stats_field::experience>(row) = m.experience = Random::next(rng); break;
            case 3: t.get<label_field::text>(row) = m.text = random_text(); break;
            case 4: t.get<label_field::tracked>(row).value = m.tracked_value = Random::next(rng); break;
            default: fm_assert(false);
            }
        }

        fm_assert(tables[0].size() == models[0].size());
        fm_assert(tables[1].size() == models[1].size());
        fm_assert(counted::num_live - num_live_before == (i32)(tables[0].size() + tables[1].size()));
        if (op % 64 == 63)
        {
            check_against_model(tables[0], models[0]);
            check_against_model(tables[1], models[1]);
        }
    }
    check_against_model(tables[0], models[0]);
    check_against_model(tables[1], models[1]);
}

} // namespace

} // namespace floormat::component_table_test

namespace floormat {

void Test::test_component_table()
{
    using namespace floormat::component_table_test;
    test_insert_keeps_rows_sorted();
    test_find_row();
    test_erase_row();
    test_move_row_to();
    test_row_refs();
    test_lifetime();
    test_random_operations();
}

} // namespace floormat
