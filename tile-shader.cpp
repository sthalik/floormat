#include "tile-shader.hpp"
#include "loader.hpp"
#include <algorithm>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>

namespace Magnum::Examples {

tile_shader::tile_shader()
{
    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(GL::Version::GL460);

    GL::Shader vert{GL::Version::GL460, GL::Shader::Type::Vertex};
    GL::Shader frag{GL::Version::GL460, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/tile-shader.vert"));
    frag.addSource(loader.shader("shaders/tile-shader.frag"));

    CORRADE_INTERNAL_ASSERT_OUTPUT(GL::Shader::compile({vert, frag}));

    attachShaders({vert, frag});

    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(ScaleUniform, Vector2{640, 480});

    samplers.reserve(MAX_SAMPLERS);
}

tile_shader& tile_shader::bind_texture(GL::Texture2D& texture, int id)
{
    texture.bind(id);
    return *this;
}

tile_shader& tile_shader::set_scale(const Vector2& scale)
{
    scale_ = scale;
    setUniform(ScaleUniform, scale);
    return *this;
}
tile_shader& tile_shader::set_camera_offset(Vector2 camera_offset)
{
    CORRADE_INTERNAL_ASSERT(std::fabs(camera_offset[0]) <= std::scalbn(1.f, std::numeric_limits<float>::digits));
    CORRADE_INTERNAL_ASSERT(std::fabs(camera_offset[1]) <= std::scalbn(1.f, std::numeric_limits<float>::digits));
    camera_offset_ = camera_offset;
    setUniform(OffsetUniform, camera_offset);
    return *this;
}
Vector2 tile_shader::project(Vector3 pt)
{
    float x = pt[1], y = pt[0], z = pt[2];
    return { x-y, (x+y+z*2)*.75f };
}

int tile_shader::bind_sampler(const tile_shader::shared_sampler& atlas)
{
    CORRADE_INTERNAL_ASSERT(samplers.size() < MAX_SAMPLERS);
    auto sampler_comparator = [](const sampler_tuple& a, const sampler_tuple& b) {
        const auto& [ptr1, n1] = a;
        const auto& [ptr2, n2] = b;
        return ptr1.get() < ptr2.get();
    };
    auto it = std::lower_bound(samplers.begin(), samplers.end(),
                               sampler_tuple{atlas, -1}, sampler_comparator);
    int idx;
    if (it == samplers.end()) {
        idx = (int)samplers.size();
        samplers.emplace_back(atlas, idx);
    } else
        idx = it->second;
    atlas->texture().bind(idx);
    return idx;
}

void tile_shader::clear_samplers()
{
    Magnum::GL::AbstractTexture::unbindImages(0, samplers.size());
    samplers.clear();
}

} // namespace Magnum::Examples
