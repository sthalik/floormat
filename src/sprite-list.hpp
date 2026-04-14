#pragma once
#include "compat/defs.hpp"
#include "quads.hpp"
#include <cr/Array.h>

namespace floormat {

struct object;

struct SpriteList
{
    Array<Quads::vertexes> Vertexes;
    Array<Quads::texcoords> UVs;
    Array<float> Depths;
    Array<object*> Objects;

    void add(const Quads::vertexes& vertexes, const Quads::texcoords& uv, float depth, object* obj);
    void clear();
    uint32_t size() const;

    SpriteList();

    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(SpriteList);
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(SpriteList);
};

} // namespace floormat
