#pragma once

#include "light-falloff.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
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
    Math::Color4<uint8_t> color;
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
    void add_chunk(Vector2i neighbor_offset, const chunk& c);
    void end();
    GL::Texture2D& texture();

private:
    static Framebuffer make_framebuffer(Vector2i size);
    GL::Mesh make_mesh();
    void add_light(Vector2i neighbor_offset, const light_s& light);
    void flush_vertexes();
    void add_quad(const std::array<Vector2, 4>& quad);
    void bind();
    void clear();
    static std::array<UnsignedShort, 6> quad_indexes(size_t N);
    static Vector2 project_vertex(Vector2 light, Vector2 vertex, Vector2 length);

    enum : int {
        ColorIntensityUniform = 0,
        CenterUniform         = 1,
        FalloffUniform        = 2,
        SizeUniform           = 3,
        //DepthUniform          = 4,
    };

    Framebuffer framebuffer, accum;
    Array<std::array<Vector2, 4>> _quads;
    Array<std::array<UnsignedShort, 6>> _indexes;
    size_t _count = 0;
    GL::Buffer _vertex_buf{NoCreate}, _index_buf{NoCreate};
    GL::Mesh _mesh{NoCreate};
    Optional<Vector2> _light_center;
};

} // namespace floormat
