#include "app.hpp"
#include "src/order-list.inl"
#include <array>
#include <initializer_list>
#include <cr/GrowableArray.h>

namespace floormat {

struct order_list_access
{
    static Array<u32> dependents(const order_list& list, u32 position)
    {
        Array<u32> ret;
        arrayReserve(ret, 16);
        const auto& nodes = list._dependent_nodes;
        for (auto i = list._order_states[position].first_dependent_node; i != (u32)-1; i = nodes[i].next)
            arrayAppend(ret, nodes[i].dependent);
        return ret;
    }

    static u32 num_unfinished_dependencies(const order_list& list, u32 position)
    {
        return list._order_states[position].num_unfinished_dependencies;
    }

    static std::array<usize, 5> capacities(order_list& list)
    {
        return {
            arrayCapacity(list._orders), arrayCapacity(list._order_states), arrayCapacity(list._dependent_nodes),
            arrayCapacity(list._ready_heap), arrayCapacity(list._buffer_objects),
        };
    }

    static std::array<usize, 4> capacities(order_buffer& buffer)
    {
        return {
            arrayCapacity(buffer._orders), arrayCapacity(buffer._dependency_counts),
            arrayCapacity(buffer._dependencies), arrayCapacity(buffer._object_orders),
        };
    }
};

} // namespace floormat

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

// Gives the object at update_index two orders, the second depending on the first.
void add_two_orders(order_buffer& b, u32 update_index, object_id giver)
{
    b.begin_object(update_index);
    const auto first = b.add({ .giver = giver });
    fm_assert_equal(0u, first);
    const u32 dependencies[] = { first };
    fm_assert_equal(1u, b.add({ .giver = giver }, dependencies));
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
    {
        fm_assert_equal(givers[p/2], list[p].giver);
        const auto dependents = order_list_access::dependents(list, p);
        if (p % 2 == 0)
        {
            fm_assert_equal(1uz, dependents.size());
            fm_assert_equal(p + 1, dependents[0]);
            fm_assert_equal(0u, order_list_access::num_unfinished_dependencies(list, p));
        }
        else
        {
            fm_assert(dependents.isEmpty());
            fm_assert_equal(1u, order_list_access::num_unfinished_dependencies(list, p));
        }
    }

    Array<u32> ran;
    list.run_all([&](order, u32 position) { arrayAppend(ran, position); });
    check_sequence(ran, { 0, 1, 2, 3, 4, 5 });
}

void test_no_dependencies()
{
    order_list list;
    for (auto i = 0u; i < 4; i++)
        fm_assert_equal(i, list.add({ .giver = 11 }));
    Array<u32> ran;
    list.run_all([&](order, u32 position) { arrayAppend(ran, position); });
    check_sequence(ran, { 0, 1, 2, 3 });
}

void test_lowest_ready_first()
{
    order_list list;
    const auto a = list.add({});
    const u32 on_a[] = { a };
    const auto b = list.add({}, on_a);
    const auto c = list.add({});
    Array<u32> ran;
    list.run_all([&](order, u32 position) { arrayAppend(ran, position); });
    check_sequence(ran, { a, b, c });
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
            const u32 on_c[] = { c };
            d = list.add({}, on_c);
        }
    });
    check_sequence(ran, { a, b, c, d });
}

void test_dependency_on_finished_order()
{
    order_list list;
    const auto a = list.add({}), b = list.add({});
    u32 c = (u32)-1;
    Array<u32> ran;
    list.run_all([&](order, u32 position) {
        arrayAppend(ran, position);
        if (position == b)
        {
            const u32 on_a[] = { a };
            c = list.add({}, on_a);
            fm_assert_equal(0u, order_list_access::num_unfinished_dependencies(list, c));
        }
    });
    check_sequence(ran, { a, b, c });
}

void test_dependency_on_running_order()
{
    order_list list;
    const auto a = list.add({}), b = list.add({});
    u32 c = (u32)-1;
    Array<u32> ran;
    list.run_all([&](order, u32 position) {
        arrayAppend(ran, position);
        if (position == a)
        {
            const u32 on_a[] = { a };
            c = list.add({}, on_a);
            fm_assert_equal(1u, order_list_access::num_unfinished_dependencies(list, c));
        }
    });
    check_sequence(ran, { a, b, c });
}

void test_two_dependencies()
{
    order_list list;
    const auto a = list.add({});
    const u32 on_a[] = { a };
    const auto b = list.add({}, on_a), c = list.add({}, on_a);
    const u32 on_b_and_c[] = { b, c };
    const auto d = list.add({}, on_b_and_c);
    fm_assert_equal(2u, order_list_access::num_unfinished_dependencies(list, d));
    Array<u32> ran;
    list.run_all([&](order, u32 position) { arrayAppend(ran, position); });
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
        {
            const u32 on_running[] = { position };
            list.add({}, on_running);
        }
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
    const auto c = list.add({});
    const u32 on_c[] = { c };
    const auto d = list.add({}, on_c);
    const u32 on_a[] = { a };
    const auto e = list.add({}, on_a);
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
    test_no_dependencies();
    test_lowest_ready_first();
    test_order_gives_orders();
    test_dependency_on_finished_order();
    test_dependency_on_running_order();
    test_two_dependencies();
    test_empty_list();
    test_clear_and_reuse();
    test_second_run_all();
    test_giver();
}

} // namespace floormat::Test
