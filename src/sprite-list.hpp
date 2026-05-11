#pragma once
#include "compat/defs.hpp"
#include "quads.hpp"
#include <cr/Array.h>

namespace floormat {

struct object;

struct SpriteList
{
    Array<Quads::vertexes> Vertexes;
    Array<float> Depths;
    Array<object*> Objects;

    void add(const Quads::vertexes& vertexes, float depth, object* obj);
    void clear();
    uint32_t size() const;

    SpriteList();

    fm_DEFAULT_MOVE_(SpriteList);
    fm_DISABLE_COPY(SpriteList);
};

} // namespace floormat
