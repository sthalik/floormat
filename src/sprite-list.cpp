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

void SpriteList::add(const Quads::vertexes& vertexes, float depth, object* obj)
{
    arrayReserve(Vertexes, 16);
    arrayReserve(Depths, 16);
    arrayReserve(Objects, 16);

    arrayAppend(Vertexes, vertexes);
    arrayAppend(Depths, depth);
    arrayAppend(Objects, obj);
}

void SpriteList::clear()
{
    arrayClear(Vertexes);
    arrayClear(Depths);
    arrayClear(Objects);
}

} //
