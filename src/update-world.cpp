#include "update-world.hpp"
#include "world.hpp"
#include "object-storage.inl"
#include "scenery.hpp"
#include "critter.hpp"
#include "light.hpp"
#include "hole.hpp"
#include "order-list.inl"
#include "compat/borrowed-ptr.inl"

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

void update_world(world& w, ArrayView<const chunk_coords_> chunks, Ns dt)
{
    auto& buffer = w.frame_order_buffer();
    auto& list = w.frame_order_list();
    fm_assert(buffer.empty() && list.size() == 0);

    const auto frame_no = w.increment_frame_no();
    w.advance_game_time(dt);
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
                    if constexpr (std::is_same_v<T, critter> || std::is_same_v<T, door_scenery>)
                    {
                        buffer.begin_object(update_index);
                        if constexpr (std::is_same_v<T, critter>)
                            e.think(dt, buffer);
                        else
                            buffer.add({.giver = e.id, .kind = order_kind::update_object});
                        buffer.end_object();
                    }
                    update_index++;
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
