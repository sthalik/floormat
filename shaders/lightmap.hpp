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
        GL::Texture2D scratch{NoCreate}, accum{NoCreate};
    };

    void begin_light(Vector2b neighbor_offset, const light_s& light);
    void add_chunk(Vector2b neighbor_offset, chunk& c);
    void add_entities(Vector2b neighbor_offset, chunk& c);
    void add_geometry(Vector2b neighbor_offset, chunk& c);
    void add_rect(Vector2b neighbor_offset, Vector2 min, Vector2 max);
    void add_rect(Vector2b neighbor_offset, Pair<Vector2, Vector2> minmax);
    void finish_light_only();
    void finish_and_blend_light();
    GL::Texture2D& scratch_texture();
    GL::Texture2D& accum_texture();
    void begin_accum();
    void end_accum();
    void bind();

private:
    enum {
        ColorIntensityUniform = 0,
        CenterUniform         = 1,
        FalloffUniform        = 2,
        SizeUniform           = 3,
        ModeUniform           = 4,
        SamplerUniform        = 5,
    };

    enum : Int {
        TextureSampler = 1,
    };

    enum ShaderMode : uint32_t {
        DrawLightmapMode  = 1,
        BlendLightmapMode = 2,
    };

    static Framebuffer make_framebuffer(Vector2i size);
    GL::Mesh make_mesh();
    void add_light(Vector2b neighbor_offset, const light_s& light);
    void flush_vertexes(ShaderMode mode);
    void add_quad(const std::array<Vector2, 4>& quad);
    void clear_scratch();
    void clear_accum();
    static std::array<UnsignedShort, 6> quad_indexes(size_t N);
    static Vector2 project_vertex(Vector2 light, Vector2 vertex, Vector2 length);

    Framebuffer framebuffer;
    Array<std::array<Vector2, 4>> _quads;
    Array<std::array<UnsignedShort, 6>> _indexes;
    size_t _count = (size_t)-1;
    GL::Buffer _vertex_buf{NoCreate}, _index_buf{NoCreate};
    GL::Mesh _mesh{NoCreate};
    Optional<Vector2> _light_center;
};

} // namespace floormat
