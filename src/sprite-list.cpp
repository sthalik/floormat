#include "sprite-list.hpp"
#include <cr/GrowableArray.h>

namespace floormat {

SpriteList::SpriteList()
{
    // a default-constructed Array is non-growable and reallocates on every append until reserved
    arrayReserve(Vertexes, 16);
    arrayReserve(Depths, 16);
    arrayReserve(Objects, 16);
}

uint32_t SpriteList::size() const
{
    return (uint32_t)Vertexes.size();
}

void SpriteList::reserve(uint32_t count)
{
    arrayReserve(Vertexes, count);
    arrayReserve(Depths, count);
    arrayReserve(Objects, count);
}

void SpriteList::add(const Quads::vertexes& vertexes, float depth, object* obj)
{
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
