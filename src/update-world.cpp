#include "update-world.hpp"
#include "world.hpp"
#include "object-storage.inl"
#include "scenery.hpp"
#include "critter.hpp"
#include "light.hpp"
#include "hole.hpp"

namespace floormat {

void update_world(world& w, ArrayView<const chunk_coords_> chunks, Ns dt)
{
    const auto frame_no = w.increment_frame_no();
    w.advance_game_time(dt);
    for (auto ch : chunks)
    {
            auto* const cʹ = w.at(ch);
            if (!cʹ)
                continue;
            auto& c = *cʹ;
            c.objects().visit_lists([&]<typename T>(object_list<T>& l) {
                auto size = l.size();
                for (auto i = 0u; i < size; i++)
                {
                    auto index = usize{i};
                    T& e = l[i];
                    if (e.last_frame_no == frame_no) [[unlikely]]
                        continue;
                    e.last_frame_no = frame_no;
                    e.update(l.ptr(i), index, dt); // objects can't delete themselves during update()
                    if (&e.chunk() != cʹ || index > i) [[unlikely]]
                    {
                        i--;
                        size = l.size();
                    }
                }
            });
    }

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
