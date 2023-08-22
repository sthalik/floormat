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
    explicit lightmap_shader();
    ~lightmap_shader() override;

    struct Framebuffer final {
        GL::Framebuffer fb{NoCreate};
        //GL::Renderbuffer depth{NoCreate};
        GL::Texture2D scratch{NoCreate}, accum{NoCreate};
    };

    struct output final {
        GL::Texture2D &scratch, &accum;
    };

#if 0
const blend_light = {
  equation: {color: gl.FUNC_ADD, alpha: gl.FUNC_ADD},
  function: {color_src:gl.DST_ALPHA, alpha_src:gl.ONE,
             color_dst:gl.ONE, alpha_dst:gl.ZERO},
};

// Shadows should only be drawn into the alpha channel and should leave color untouched.
// You could also do this with a write mask if that's supported.
const blend_shadow = {
  equation: {color: gl.FUNC_ADD, alpha: gl.FUNC_ADD},
  function: {color_src:gl.ZERO, alpha_src:gl.ZERO,
             color_dst:gl.ONE_MINUS_SRC_COLOR, alpha_dst:GL_ONE},
};
#endif

    //void begin_light(Vector2 neighbor_offset, const light_s& light);

    void begin_occlusion();
    void end_occlusion();
    void add_chunk(Vector2 neighbor_offset, chunk& c);
    void add_entities(Vector2 neighbor_offset, chunk& c);
    void add_geometry(Vector2 neighbor_offset, chunk& c);
    void add_rect(Vector2 neighbor_offset, Vector2 min, Vector2 max);
    void add_rect(Vector2 neighbor_offset, Pair<Vector2, Vector2> minmax);
    //void finish_light_only();
    //void finish_and_blend_light();
    struct output output();

    //GL::Texture2D& scratch_texture();
    //GL::Texture2D& accum_texture();
    //void begin_accum();
    //void end_accum();
    void bind();

    static constexpr auto max_chunks = Vector2s(8, 8);

#if 0
layout (location = 0) uniform sampler2D sampler;
layout (location = 1) uniform vec3 light_color;
layout (location = 2) uniform vec2 size;
layout (location = 3) uniform vec2 center_fragcoord;
layout (location = 4) uniform vec2 center_clip;
layout (location = 5) uniform float intensity;
layout (location = 6) uniform uint mode;
layout (location = 7) uniform uint falloff;
#endif

    using Position = GL::Attribute<0, Vector2>;

private:
    enum : Int {
        SamplerUniform         = 0,
        LightColorUniform      = 1,
        SizeUniform            = 2,
        CenterFragcoordUniform = 3,
        CenterClipUniform      = 4,
        IntensityUniform       = 5,
        ModeUniform            = 6,
        FalloffUniform         = 7,
    };

    enum : Int {
        TextureSampler = 1,
    };

    enum ShaderMode : uint32_t
    {
        DrawShadowsMode   = 0,
        DrawLightmapMode  = 1,
        BlendLightmapMode = 2,
    };

#if 0
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{},
            tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
#endif

    static Framebuffer make_framebuffer(Vector2i size);
    GL::Mesh make_occlusion_mesh();
    void add_light(Vector2 neighbor_offset, const light_s& light);
    //void flush_vertexes(ShaderMode mode);
    //void add_quad(const std::array<Vector2, 4>& quad);
    //void clear_scratch();
    //void clear_accum();
    static std::array<UnsignedShort, 6> quad_indexes(size_t N);

    // todo use setData() and a boolean flag on capacity change
    GL::Buffer vertex_buf{NoCreate}, index_buf{NoCreate};
    Array<std::array<Vector3, 4>> vertexes; // todo make a contiguous allocation
    Array<std::array<UnsignedShort, 6>> indexes;
    size_t count = 0, capacity = 0;
    Framebuffer framebuffer;
    GL::Mesh occlusion_mesh{NoCreate};
    static constexpr auto starting_capacity = 1; // todo

    std::array<Vector3, 4> light_vertexes;
    GL::Buffer light_vertex_buf{NoCreate};
    GL::Mesh light_mesh{NoCreate};

    GL::Mesh blend_mesh{NoCreate};

    [[nodiscard]] std::array<Vector3, 4>& alloc_rect();
};

} // namespace floormat
