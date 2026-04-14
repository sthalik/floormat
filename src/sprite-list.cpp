#include "sprite-list.hpp"
#include <cr/GrowableArray.h>

namespace floormat {

SpriteList::SpriteList()
{
}

uint32_t SpriteList::size() const
{
    return (uint32_t)Vertexes.size();
}

void SpriteList::add(const Quads::vertexes& vertexes, const Quads::texcoords& uv, float depth, object* obj)
{
    arrayReserve(Vertexes, 16);
    arrayReserve(UVs, 16);
    arrayReserve(Depths, 16);
    arrayReserve(Objects, 16);

    arrayAppend(Vertexes, vertexes);
    arrayAppend(UVs, uv);
    arrayAppend(Depths, depth);
    arrayAppend(Objects, obj);
}

void SpriteList::clear()
{
    arrayClear(Vertexes);
    arrayClear(UVs);
    arrayClear(Depths);
    arrayClear(Objects);
}

} //
