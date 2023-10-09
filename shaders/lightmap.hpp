#pragma once

#include "src/light-falloff.hpp"
#include "shaders/texture-unit-cache.hpp"
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

struct texture_unit_cache;

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
    explicit lightmap_shader(texture_unit_cache& tuc);
    ~lightmap_shader() override;

    struct Framebuffer final {
        GL::Framebuffer fb{NoCreate};
        //GL::Renderbuffer depth{NoCreate};
        GL::Texture2D scratch{NoCreate}, accum{NoCreate};
    };

    void begin_occlusion();
    void end_occlusion();
    void add_chunk(Vector2 neighbor_offset, chunk& c);
    void add_light(Vector2 neighbor_offset, const light_s& light);
    void bind();
    void finish();
    static int iter_bounds();

    GL::Texture2D& accum_texture();

    using Position = GL::Attribute<0, Vector3>;

private:
    enum : Int {
        SamplerUniform         = 2,
        ModeUniform            = 3,
        // GL_MAX_UNIFORM_BUFFER_BINDINGS must be at least 36
        BlockUniform           = 35,
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

    void add_objects(Vector2 neighbor_offset, chunk& c);
    void add_geometry(Vector2 neighbor_offset, chunk& c);
    void add_rect(Vector2 neighbor_offset, Vector2 min, Vector2 max);
    void add_rect(Vector2 neighbor_offset, Pair<Vector2, Vector2> minmax);
    [[nodiscard]] std::array<Vector3, 4>& alloc_rect();

    texture_unit_cache& tuc; // NOLINT(*-avoid-const-or-ref-data-members)
    GL::Buffer vertex_buf{NoCreate}, index_buf{NoCreate},
               block_uniform_buf{GL::Buffer::TargetHint::Uniform, };
    Array<std::array<Vector3, 4>> vertexes; // todo make a contiguous allocation
    Array<std::array<UnsignedShort, 6>> indexes;
    size_t count = 0, capacity = 0;
    Framebuffer framebuffer;
    GL::Mesh occlusion_mesh{NoCreate};
    static constexpr auto starting_capacity = 1; // todo

    GL::Buffer light_vertex_buf{NoCreate};
    GL::Mesh light_mesh{NoCreate};
};

} // namespace floormat
