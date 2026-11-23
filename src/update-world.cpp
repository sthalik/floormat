#include "update-world.hpp"
#include "world.hpp"
#include "object-storage.inl"
#include "scenery.hpp"
#include "critter.hpp"
#include "light.hpp"
#include "hole.hpp"
#include "order-list.inl"
#include "grid-order.hpp"
#include "grid-registry.hpp"
#include "compat/borrowed-ptr.inl"
#include <cr/GrowableArray.h>

namespace floormat {

void run_order(world& w, const order& o, const Ns& dt)
{
    const auto objʹ = w.find_object(o.giver);
    fm_assert(objʹ);
    auto& obj = *objʹ;
    usize i = obj.index();
    switch (o.kind)
    {
    case order_kind::none:
        return;
    case order_kind::update_object:
        fm_assert(obj.type() != object_type::critter);
        obj.update(objʹ, i, dt);
        return;
    case order_kind::script_order:
    case order_kind::move_by_keys:
        fm_assert(obj.type() == object_type::critter);
        static_cast<critter&>(obj).run_order(static_pointer_cast<critter>(objʹ), i, dt, o);
        return;
    case order_kind::COUNT:
        break;
    }
    fm_assert(false);
}

namespace {

// Insertion sort, since std::stable_sort would allocate a buffer per call.
void sort_by_update_index(ArrayView<grid_order> orders)
{
    for (u32 i = 1; i < (u32)orders.size(); i++)
    {
        const auto o = orders[i];
        auto j = i;
        for (; j > 0 && orders[j-1].update_index > o.update_index; j--)
            orders[j] = orders[j-1];
        orders[j] = o;
    }
}

} // namespace

void run_grid_orders(world& w)
{
    auto& orders = w.grid_orders();
    if (orders.isEmpty())
        return;
    auto* reg = w.grid_registry();
    fm_assert(reg);
    sort_by_update_index(orders);

    u32 num_octant_fills = 0;
    // As in Cover::Point::finder::find(), the cap is tested before a rebuild and again before an octant fill.
    const auto is_octant_fill_cap_reached = [&] {
        return num_octant_fills >= Grid::Cover::max_octant_fills_per_frame;
    };
    for (u32 i = 0; i < (u32)orders.size(); i++)
    {
        const auto& o = orders[i];
        if (i > 0 && o.update_index != orders[i-1].update_index)
            num_octant_fills = 0;
        auto* c = w.at(o.chunk);
        if (!c)
            continue;
        switch (o.kind)
        {
        case grid_order::grid_kind::pass:
            reg->pass_pool(o.pass_params)[*c].build_if_stale();
            break;
        case grid_order::grid_kind::object:
        {
            auto& pool = reg->object_pool(o.pass_params);
            pool[*c].build_if_stale();
            pool.pass_pool()[*c].build_if_stale();
            break;
        }
        case grid_order::grid_kind::cover:
        {
            if (is_octant_fill_cap_reached())
                break;
            auto g = reg->cover_pool(Grid::Cover::Params{o.cover_div_size})[*c];
            if (g.is_marked_stale())
            {
                g.build_if_stale();
                num_octant_fills++;
            }
            if (!is_octant_fill_cap_reached() && !(g.built_octants() & 1u << o.octant))
                num_octant_fills += g.ensure_octant(o.octant);
            break;
        }
        }
    }
    arrayClear(orders);
}

void prepare_world_for_thinking(world& w)
{
    w.ensure_passability();
    if (auto* reg = w.grid_registry())
    {
        reg->rebuild_requested_stale_grids();
        run_grid_orders(w);
    }
    else
        fm_assert(w.grid_orders().isEmpty());
}

void update_world(world& w, ArrayView<const chunk_coords_> chunks, Ns dt)
{
    auto& buffer = w.frame_order_buffer();
    auto& list = w.frame_order_list();
    fm_assert(buffer.empty() && list.size() == 0);

    const auto frame_no = w.increment_frame_no();
    w.advance_game_time(dt);
    prepare_world_for_thinking(w);
    auto ctx = think_context{buffer, w.grid_orders()};
    u32 update_index = 0;
    for (auto ch : chunks)
    {
            auto* const cʹ = w.at(ch);
            if (!cʹ)
                continue;
            auto& c = *cʹ;
            c.objects().visit_lists([&]<typename T>(object_list<T>& l) {
                const auto size = l.size();
                for (auto i = 0u; i < size; i++)
                {
                    T& e = l[i];
                    if (e.last_frame_no == frame_no) [[unlikely]]
                        continue;
                    e.last_frame_no = frame_no;
                    // Generic scenery, lights and holes give no orders, since their update() is empty.
                    if constexpr (std::is_same_v<T, critter>)
                    {
                        e.think(dt, ctx, update_index);
                        update_index++;
                    }
                    else if constexpr (std::is_same_v<T, door_scenery>)
                    {
                        buffer.begin_object(update_index);
                        buffer.add({.giver = e.id, .kind = order_kind::update_object});
                        buffer.end_object();
                        update_index++;
                    }
                }
            });
    }

    list.append_buffers({&buffer, 1});
    list.run_all([&](const order& o, u32) { run_order(w, o, dt); });
    list.clear();
    buffer.clear();

#if 0
#ifndef FM_NO_DEBUG
    for (auto ch : chunks)
    {
        auto* cʹ = w.at(ch);
        if (!cʹ)
            continue;
        auto& c = *cʹ;
        const auto& es = c.objects();
        auto size = es.size();
        for (auto i = 0uz; i < size; i++)
        {
            auto& e = *es[i];
            fm_assert(e.last_frame_no == frame_no);
        }
    }
#endif
#endif
}

} // namespace floormat
