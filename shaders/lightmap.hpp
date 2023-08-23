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

    void begin_occlusion();
    void end_occlusion();
    void add_chunk(Vector2 neighbor_offset, chunk& c);
    void add_entities(Vector2 neighbor_offset, chunk& c);
    void add_geometry(Vector2 neighbor_offset, chunk& c);
    void add_rect(Vector2 neighbor_offset, Vector2 min, Vector2 max);
    void add_rect(Vector2 neighbor_offset, Pair<Vector2, Vector2> minmax);
    void add_light(const light_s& light);
    void bind();

    GL::Texture2D& scratch_texture();
    GL::Texture2D& accum_texture();

    static constexpr auto max_chunks = Vector2s(8, 8);

    using Position = GL::Attribute<0, Vector3>;

private:
    enum : Int {
        SamplerUniform         = 0,
        LightColorUniform      = 1,
        SizeUniform            = 2,
        CenterFragcoordUniform = 3,
        CenterClipUniform      = 4,
        RangeUniform           = 5,
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

    static Framebuffer make_framebuffer(Vector2i size);
    GL::Mesh make_occlusion_mesh();
    static std::array<UnsignedShort, 6> quad_indexes(size_t N);

    // todo use setData() and a boolean flag on capacity change
    GL::Buffer vertex_buf{NoCreate}, index_buf{NoCreate};
    Array<std::array<Vector3, 4>> vertexes; // todo make a contiguous allocation
    Array<std::array<UnsignedShort, 6>> indexes;
    size_t count = 0, capacity = 0;
    Framebuffer framebuffer;
    GL::Mesh occlusion_mesh{NoCreate};
    static constexpr auto starting_capacity = 1; // todo

    GL::Mesh blend_mesh{NoCreate};

    [[nodiscard]] std::array<Vector3, 4>& alloc_rect();
};

} // namespace floormat
