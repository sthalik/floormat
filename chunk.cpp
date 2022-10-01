#include "chunk.hpp"

namespace Magnum::Examples {

chunk_sampler_array::chunk_sampler_array()
{
    samplers.reserve(MAX_SAMPLERS);
}

void chunk_sampler_array::ensure_sampler(std::size_t tile_id, const shared_sampler& x)
{
    CORRADE_INTERNAL_ASSERT(tile_id < TILE_COUNT);
    if (std::size_t id = sampler_map[tile_id]; id != 0)
    {
        const shared_sampler& old_sampler = samplers[id];
        if (x == old_sampler)
            return;
    }
    CORRADE_INTERNAL_ASSERT(samplers.size() < MAX_SAMPLERS);
    const auto id = (std::uint8_t)samplers.size();
    samplers.push_back(x);
    sampler_map[tile_id] = id;
}

void chunk_sampler_array::clear()
{
    Magnum::GL::AbstractTexture::unbindImages(0, samplers.size());
    samplers.clear();
    sampler_map = {};
}

void chunk_sampler_array::bind()
{
    Magnum::GL::AbstractTexture::unbindImages(0, MAX_SAMPLERS);
    for (std::size_t i = 0; i < samplers.size(); i++)
        samplers[i]->texture().bind((int)i + 1);
}

std::shared_ptr<tile_atlas> chunk_sampler_array::operator[](std::size_t tile_id) const
{
    CORRADE_INTERNAL_ASSERT(tile_id < TILE_COUNT);
    std::size_t sampler_id = sampler_map[tile_id] - 1;
    CORRADE_INTERNAL_ASSERT(sampler_id < samplers.size());
    const auto& sampler = samplers[sampler_id];
    CORRADE_INTERNAL_ASSERT(sampler != nullptr);
    return sampler;
}

} // namespace Magnum::Examples
