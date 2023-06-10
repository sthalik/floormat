#pragma once

#include "light-falloff.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Mesh.h>
//#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

struct light_s final
{
    Vector2 center;
    float dist = 1;
    //float depth = -1 + 1e-4f;
    Math::Color3<uint8_t> color {255, 255, 255};
    light_falloff falloff = light_falloff::linear;

    bool operator==(const light_s&) const noexcept;
};

struct chunk;

struct lightmap_shader final : GL::AbstractShaderProgram
{
    using Position = GL::Attribute<0, Vector2>;

    explicit lightmap_shader();
    ~lightmap_shader() override;

    struct Framebuffer final {
        GL::Framebuffer fb{NoCreate};
        //GL::Renderbuffer depth{NoCreate};
        GL::Texture2D color{NoCreate};
    };

    void begin(Vector2i neighbor_offset, const light_s& light);
    void add_chunk(Vector2i neighbor_offset, const chunk& ch);
    void add_vertex(Vector2i neighbor_offset, const std::array<Vector2, 4>& obj);
    void end();
    GL::Texture2D& texture();

private:
    static Framebuffer make_framebuffer();
    GL::Mesh make_mesh();
    void add_light(Vector2i neighbor_offset, const light_s& light);
    void flush_vertexes();
    void bind();
    void clear();
    static std::array<UnsignedShort, 6> quad_indexes(size_t N);

    enum : int {
        ColorIntensityUniform = 0,
        CenterUniform         = 1,
        FalloffUniform        = 2,
        SizeUniform           = 3,
        //DepthUniform          = 4,
    };

    Framebuffer framebuffer;
    Array<std::array<Vector2, 4>> _quads;
    Array<std::array<UnsignedShort, 6>> _indexes;
    size_t _count = 0;
    //light_u _light_uniform;
    //light_s _light;
    GL::Buffer _vertex_buf{NoCreate}, _index_buf{NoCreate};
    GL::Mesh _mesh{NoCreate};
};

} // namespace floormat
