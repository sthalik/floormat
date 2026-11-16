#pragma once
#include "compat/defs.hpp"
#include "quads.hpp"
#include <cr/Array.h>

namespace floormat {

struct object;

struct SpriteList
{
    Array<Quads::vertexes> Vertexes;
    Array<u32> Depths;

    void add(const Quads::vertexes& vertexes, u32 depth);
    void clear();
    void reserve(u32 count);
    u32 size() const;

    SpriteList();

    fm_DEFAULT_MOVE_(SpriteList);
    fm_DISABLE_COPY(SpriteList);
};

} // namespace floormat
