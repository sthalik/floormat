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

void SpriteList::add(const Quads::vertexes& vertexes, GL::Texture2D* texture, float depth, object* obj)
{
    arrayReserve(Vertexes, 16);
    arrayReserve(Textures, 16);
    arrayReserve(Depths, 16);
    arrayReserve(Objects, 16);

    arrayAppend(Vertexes, vertexes);
    arrayAppend(Textures, texture);
    arrayAppend(Depths, depth);
    arrayAppend(Objects, obj);
}

void SpriteList::clear()
{
    arrayClear(Vertexes);
    arrayClear(Textures);
    arrayClear(Depths);
    arrayClear(Objects);
}

} //
