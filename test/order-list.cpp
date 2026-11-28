#include "app.hpp"
#include "order-list-access.hpp"
#include "src/order-list.inl"
#include <array>
#include <initializer_list>
#include <cr/GrowableArray.h>

namespace floormat::Test {

namespace {

void check_sequence(ArrayView<const u32> ran, std::initializer_list<u32> expected)
{
    fm_assert_equal(expected.size(), ran.size());
    auto i = 0u;
    for (auto position : expected)
        fm_assert_equal(position, ran[i++]);
}

template<usize N>
void check_not_shrunk(const std::array<usize, N>& before, const std::array<usize, N>& after)
{
    for (auto i = 0u; i < N; i++)
        fm_assert(after[i] >= before[i]);
}

void add_two_orders(order_buffer& b, u32 update_index, object_id giver)
{
    b.begin_object(update_index);
    fm_assert_equal(0u, b.add({ .giver = giver }));
    fm_assert_equal(1u, b.add({ .giver = giver }));
    b.end_object();
}

void test_merge_order()
{
    constexpr object_id givers[] = { 31, 37, 41 };
    order_buffer buffers[2];
    add_two_orders(buffers[0], 2, givers[2]);
    add_two_orders(buffers[0], 0, givers[0]);
    add_two_orders(buffers[1], 1, givers[1]);

    order_list list;
    list.append_buffers(buffers);
    fm_assert_equal(6u, list.size());
    for (auto p = 0u; p < 6; p++)
        fm_assert_equal(givers[p/2], list[p].giver);

    Array<u32> ran;
    list.run_all([&](order, u32 position) { arrayAppend(ran, position); });
    check_sequence(ran, { 0, 1, 2, 3, 4, 5 });
}

void test_order_gives_orders()
{
    order_list list;
    const auto a = list.add({}), b = list.add({});
    u32 c = (u32)-1, d = (u32)-1;
    Array<u32> ran;
    list.run_all([&](order, u32 position) {
        arrayAppend(ran, position);
        if (position == a)
        {
            c = list.add({});
            d = list.add({});
        }
    });
    check_sequence(ran, { a, b, c, d });
}

void test_empty_list()
{
    order_list list;
    list.run_all([](order, u32) { fm_assert(false); });
    order_buffer buffers[2];
    buffers[1].begin_object(3);
    buffers[1].end_object();
    fm_assert(buffers[0].empty() && buffers[1].empty());
    list.append_buffers(buffers);
    fm_assert_equal(0u, list.size());
    list.run_all([](order, u32) { fm_assert(false); });
}

void test_clear_and_reuse()
{
    order_buffer buffers[2];
    order_list list;
    Array<u32> ran;
    const auto record = [&](order, u32 position) { arrayAppend(ran, position); };

    for (auto i = 0u; i < 23; i++)
        add_two_orders(buffers[i % 2], 22 - i, 43 + i);
    list.append_buffers(buffers);
    const auto num_merged = list.size();
    fm_assert_equal(46u, num_merged);
    list.run_all([&](order o, u32 position) {
        record(o, position);
        if (position < num_merged)
            list.add({});
    });
    fm_assert_equal(92uz, ran.size());

    const auto list_capacities = order_list_access::capacities(list);
    const auto buffer_capacities = std::array{
        order_list_access::capacities(buffers[0]), order_list_access::capacities(buffers[1]),
    };
    list.clear();
    buffers[0].clear();
    buffers[1].clear();
    fm_assert_equal(0u, list.size());
    fm_assert(buffers[0].empty() && buffers[1].empty());
    check_not_shrunk(list_capacities, order_list_access::capacities(list));
    check_not_shrunk(buffer_capacities[0], order_list_access::capacities(buffers[0]));
    check_not_shrunk(buffer_capacities[1], order_list_access::capacities(buffers[1]));

    arrayClear(ran);
    constexpr object_id givers[] = { 53, 59, 61 };
    add_two_orders(buffers[1], 1, givers[1]);
    add_two_orders(buffers[0], 2, givers[2]);
    add_two_orders(buffers[1], 0, givers[0]);
    list.append_buffers(buffers);
    fm_assert_equal(6u, list.size());
    for (auto p = 0u; p < 6; p++)
        fm_assert_equal(givers[p/2], list[p].giver);
    list.run_all(record);
    check_sequence(ran, { 0, 1, 2, 3, 4, 5 });
    check_not_shrunk(list_capacities, order_list_access::capacities(list));
    check_not_shrunk(buffer_capacities[0], order_list_access::capacities(buffers[0]));
    check_not_shrunk(buffer_capacities[1], order_list_access::capacities(buffers[1]));
}

void test_second_run_all()
{
    order_list list;
    const auto a = list.add({}), b = list.add({});
    Array<u32> ran;
    const auto record = [&](order, u32 position) { arrayAppend(ran, position); };
    list.run_all(record);
    check_sequence(ran, { a, b });

    arrayClear(ran);
    const auto c = list.add({}), d = list.add({}), e = list.add({});
    list.run_all(record);
    check_sequence(ran, { c, d, e });
}

void test_giver()
{
    order_list list;
    const auto a = list.add({ .giver = 7 }), b = list.add({ .giver = 0 });
    fm_assert_equal(object_id{0}, list[b].giver);
    u32 c = (u32)-1, d = (u32)-1, e = (u32)-1, f = (u32)-1;
    list.run_all([&](order o, u32 position) {
        if (position == a)
        {
            fm_assert_equal(object_id{7}, o.giver);
            c = list.add({ .giver = 0 });
            d = list.add({ .giver = 9 });
        }
        else if (position == b)
            e = list.add({ .giver = 0 });
        else if (position == c)
            f = list.add({ .giver = 0 });
    });
    fm_assert_equal(object_id{7}, list[c].giver);
    fm_assert_equal(object_id{9}, list[d].giver);
    fm_assert_equal(object_id{0}, list[e].giver);
    fm_assert_equal(object_id{7}, list[f].giver);
    fm_assert_equal(object_id{0}, list[list.add({ .giver = 0 })].giver);

    order_buffer buffer;
    buffer.begin_object(0);
    buffer.add({ .giver = 0 });
    buffer.end_object();
    const auto g = list.size();
    list.append_buffers({ &buffer, 1 });
    fm_assert_equal(object_id{0}, list[g].giver);
}

} // namespace

void test_order_list()
{
    test_merge_order();
    test_order_gives_orders();
    test_empty_list();
    test_clear_and_reuse();
    test_second_run_all();
    test_giver();
}

} // namespace floormat::Test
