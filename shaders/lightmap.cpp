#include "shaders/lightmap.hpp"
#include "compat/assert.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "src/chunk.hpp"
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/Iterable.h>
#include <cmath>
#include <Magnum/Magnum.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include "src/tile-bbox.hpp"
#include "src/tile-atlas.hpp"
#include <Magnum/GL/Renderer.h>

#if defined __CLION_IDE__ || defined __clang__
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

namespace floormat {

namespace {

constexpr auto max_neighbors = 8;

constexpr auto chunk_size   = TILE_SIZE2 * TILE_MAX_DIM;
constexpr auto chunk_offset = TILE_SIZE2/2;
constexpr auto image_size   = max_neighbors * iTILE_SIZE2 * TILE_MAX_DIM;

constexpr auto buffer_size = 256uz;

constexpr auto clip_start = Vector2{-1, -1};
constexpr auto clip_scale = 2/(chunk_size * max_neighbors);

constexpr auto shadow_length = chunk_size * 2 * max_neighbors;
constexpr auto shadow_color = Vector4{0, 0, 0, 1};
constexpr auto shadow_wall_depth = 4.f;

} // namespace

auto lightmap_shader::make_framebuffer(Vector2i size) -> Framebuffer
{
    Framebuffer framebuffer;

    framebuffer.scratch = GL::Texture2D{};
    framebuffer.scratch
        .setWrapping(GL::SamplerWrapping::ClampToBorder)
        .setBorderColor(Color4{0, 0, 0, 1})
        .setStorage(1, GL::TextureFormat::RGB8, size);

    framebuffer.accum = GL::Texture2D{};
    framebuffer.accum
        .setWrapping(GL::SamplerWrapping::ClampToBorder)
        .setBorderColor(Color4{0, 0, 0, 1})
        .setStorage(1, GL::TextureFormat::RGB8, size);

    //framebuffer.depth = GL::Renderbuffer{};
    //framebuffer.depth.setStorage(GL::RenderbufferFormat::DepthComponent32F, size);

    framebuffer.fb = GL::Framebuffer{{ {}, size }};
    framebuffer.fb
        //.attachRenderbuffer(GL::Framebuffer::BufferAttachment::Depth, framebuffer.depth);
        .attachTexture(GL::Framebuffer::ColorAttachment{0}, framebuffer.scratch, 0)
        .attachTexture(GL::Framebuffer::ColorAttachment{1}, framebuffer.accum, 0)
        //.clearDepth(0);
        .clearColor(0, Color4{0, 0, 0, 1})
        .clearColor(1, Color4{0, 0, 0, 1});
    framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{0});

    return framebuffer;
}

GL::Mesh lightmap_shader::make_mesh()
{
    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(_vertex_buf, 0, Position{})
        .setIndexBuffer(_index_buf, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * buffer_size));
    return mesh;
}

lightmap_shader::lightmap_shader() :
    framebuffer { make_framebuffer(image_size) },
    _quads { ValueInit, buffer_size },
    _indexes { ValueInit, buffer_size },
    _vertex_buf { _quads },
    _index_buf { _indexes },
    _mesh { make_mesh() }
{
    constexpr auto min_version = GL::Version::GL330;
    const auto version = GL::Context::current().version();

    if (version < min_version)
        fm_abort("floormat requires OpenGL version %d, only %d is supported", (int)min_version, (int)version);

    GL::Shader vert{version, GL::Shader::Type::Vertex};
    GL::Shader frag{version, GL::Shader::Type::Fragment};

    vert.addSource(loader.shader("shaders/lightmap.vert"));
    frag.addSource(loader.shader("shaders/lightmap.frag"));

    CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
    CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());
    attachShaders({vert, frag});
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());

    setUniform(ModeUniform, DrawLightmapMode);
    clear_scratch();
    clear_accum();
}

void lightmap_shader::flush_vertexes(ShaderMode mode)
{
    fm_assert(_count != (size_t)-1);

    if (_count > 0)
    {
        setUniform(ModeUniform, mode);

        _index_buf.setSubData(0, ArrayView<std::array<UnsignedShort, 6>>{_indexes, _count});
        _vertex_buf.setSubData(0, ArrayView<std::array<Vector2, 4>>{_quads, _count});

        GL::MeshView mesh{_mesh};
        mesh.setCount((int)(6 * _count));
        mesh.setIndexRange(0, 0, (uint32_t)(_count * 6 - 1));
        AbstractShaderProgram::draw(mesh);
    }
    _count = 0;
}

std::array<UnsignedShort, 6> lightmap_shader::quad_indexes(size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

void lightmap_shader::add_light(Vector2b neighbor_offset, const light_s& light)
{
    fm_debug_assert(_count == 0);
    fm_debug_assert(_quads.size() > 0);
    fm_assert(!_light_center);

    constexpr auto tile_size = TILE_SIZE2.sum()/2;
    float I;
    switch (light.falloff)
    {
    default:
        I = 1;
        break;
    case light_falloff::linear:
    case light_falloff::quadratic:
        I = light.dist * tile_size;
        break;
    }

    I = std::fmax(1.f, I);

    auto I_clip = I * tile_size;
    auto center = light.center + chunk_offset + Vector2(neighbor_offset)*chunk_size;
    auto center_clip = clip_start + Vector2{center} * clip_scale; // clip coordinates
    auto center_fragcoord = center; // window-relative coordinates

    _indexes[0] = quad_indexes(0);
    _quads[0] = std::array<Vector2, 4>{{
        {  I_clip + center_clip.x(), -I_clip + center_clip.y() },
        {  I_clip + center_clip.x(),  I_clip + center_clip.y() },
        { -I_clip + center_clip.x(), -I_clip + center_clip.y() },
        { -I_clip + center_clip.x(),  I_clip + center_clip.y() },
    }};
    _count = 1;

    float alpha = light.color.a() / 255.f;
    auto color = Vector3{light.color.rgb()} / 255.f;

    setUniform(ColorIntensityUniform, Vector4{Vector3{color} * alpha, I });
    setUniform(CenterUniform, center_fragcoord);
    setUniform(FalloffUniform, (uint32_t)light.falloff);
    setUniform(SizeUniform, 1 / (chunk_size * max_neighbors));

    _light_center = center;
    flush_vertexes(DrawLightmapMode);

    setUniform(FalloffUniform, (uint32_t)light_falloff::constant);
    setUniform(ColorIntensityUniform, shadow_color);
    setUniform(SamplerUniform, TextureSampler);
}

Vector2 lightmap_shader::project_vertex(Vector2 light, Vector2 vertex, Vector2 length)
{
    auto dir = vertex - light;
    auto len = dir.length();
    if (std::fabs(len) < 1e-4f)
        return vertex;
    auto dir_norm = dir * (1/len);
    auto ret = vertex + dir_norm * length;
    return ret;
}

void lightmap_shader::add_rect(Vector2b neighbor_offset, Vector2 min, Vector2 max)
{
    fm_assert(_light_center && _count != (size_t)-1);

    auto li = *_light_center;

    auto off = Vector2(neighbor_offset)*chunk_size + chunk_offset;
    min += off;
    max += off;

    const auto vertexes = std::array<Vector2, 4>{{
        { max.x(), min.y() },
        { max.x(), max.y() },
        { min.x(), min.y() },
        { min.x(), max.y() },
    }};
    struct pair { uint8_t first, second; };
    constexpr std::array<pair, 4> from = {{
        { 3, 1 }, // side #1: 3 -> 2, 1 -> 0
        { 1, 0 }, // side #2: 1 -> 3, 0 -> 2
        { 0, 2 }, // side #3: 0 -> 1, 2 -> 3
        { 2, 3 }, // side #4: 2 -> 0, 3 -> 1
    }};
    constexpr std::array<pair, 4> to = {{
        { 2, 0 }, /* 3--1  1 */
        { 3, 2 }, /* | /  /| */
        { 1, 3 }, /* |/  / | */
        { 0, 1 }, /* 2  2--0 */
    }};
    for (auto i = 0uz; i < 4; i++)
    {
        auto [src1, src2] = from[i];
        auto [dest1, dest2] = to[i];
        auto verts = vertexes;
        verts[dest1] = project_vertex(li, vertexes[src1], shadow_length);
        verts[dest2] = project_vertex(li, vertexes[src2], shadow_length);
        for (auto& x : verts)
            x = clip_start + x * clip_scale;
        add_quad(verts);
    }
}

void lightmap_shader::add_rect(Vector2b neighbor_offset, Pair<Vector2, Vector2> minmax)
{
    fm_assert(_light_center && _count != (size_t)-1);

    auto [min, max] = minmax;
    add_rect(neighbor_offset, min, max);
}

void lightmap_shader::add_chunk(Vector2b neighbor_offset, chunk& c)
{
    add_geometry(neighbor_offset, c);
    add_entities(neighbor_offset, c);
}

void lightmap_shader::add_geometry(Vector2b neighbor_offset, chunk& c)
{
    fm_assert(_light_center && _count != (size_t)-1);

    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        auto t = c[i];
        if (auto atlas = t.ground_atlas())
            if (atlas->pass_mode(pass_mode::pass) == pass_mode::blocked)
                add_rect(neighbor_offset, whole_tile(i));
        if (auto atlas = t.wall_north_atlas())
            if (atlas->pass_mode(pass_mode::blocked) == pass_mode::blocked)
            {
                // todo check backface
                auto start = tile_start(i);
                auto min = start - Vector2(0, shadow_wall_depth),
                     max = start + Vector2(TILE_SIZE2[0], 0);
                add_rect(neighbor_offset, {min, max});
            }
        if (auto atlas = t.wall_west_atlas())
            if (atlas->pass_mode(pass_mode::blocked) == pass_mode::blocked)
            {
                // todo check backface
                auto start = tile_start(i);
                auto min = start - Vector2(shadow_wall_depth, 0),
                     max = start + Vector2(0, TILE_SIZE[1]);
                add_rect(neighbor_offset, {min, max});
            }
    }
}

void lightmap_shader::add_entities(Vector2b neighbor_offset, chunk& c)
{
    fm_assert(_light_center && _count != (size_t)-1);

    for (const auto& e_ : c.entities())
    {
        const auto& e = *e_;
        if (e.is_virtual())
            continue;
        if (e.pass == pass_mode::pass || e.pass == pass_mode::see_through)
            continue;
        auto center = Vector2(e.offset) + Vector2(e.bbox_offset) +
                      Vector2(e.coord.local()) * TILE_SIZE2;
        auto half = Vector2(e.bbox_size)*.5f;
        auto min = center - half, max = center + half;

        add_rect(neighbor_offset, min, max);
    }
}

void lightmap_shader::add_quad(const std::array<Vector2, 4>& quad)
{
    fm_debug_assert(_count < buffer_size);
    const auto i = _count++;
    _quads[i] = quad;
    _indexes[i] = quad_indexes(i);
    if (i+1 == buffer_size) [[unlikely]]
        flush_vertexes(DrawLightmapMode);
}

void lightmap_shader::clear_scratch()
{
    _light_center = {};
    framebuffer.fb.clearColor(0, Color4{0, 0, 0, 0});
}

void lightmap_shader::clear_accum()
{
    fm_assert(!_light_center && _count == (size_t)-1);
    _count = (size_t)-1;
    //framebuffer.fb.clearColor(1, Color4{0, 0, 0, 0});
}

void lightmap_shader::bind()
{
    //fm_assert(_count == 0 && !_light_center);
    framebuffer.scratch.bind(TextureSampler);
    framebuffer.fb.bind();
}

void lightmap_shader::begin_accum()
{
    fm_assert(!_light_center);
    fm_assert(_count == (size_t)-1);

    clear_accum();
    _count = 0;

    framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{1});
    framebuffer.fb.clearColor(0, Color4{0, 0, 0, 0});
    //framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{0});
    //framebuffer.fb.clearColor(1, Color4{0, 0, 0, 0});
}

void lightmap_shader::end_accum()
{
    fm_assert(!_light_center);
    fm_assert(_count == 0);
    _count = (size_t)-1;
}

void lightmap_shader::begin_light(Vector2b neighbor_offset, const light_s& light)
{
    fm_assert(_count == 0 && !_light_center);
    clear_scratch();
    _count = 0;
    framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{0});
    framebuffer.fb.clearColor(0, Color4{0, 0, 0, 1});
    add_light(neighbor_offset, light);
    flush_vertexes(DrawLightmapMode);
}

void lightmap_shader::finish_light_only()
{
    fm_assert(_light_center && _count != (size_t)-1);
    framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{0});
    flush_vertexes(DrawLightmapMode);
    _light_center = {};
    _count = (size_t)0;
}

void lightmap_shader::finish_and_blend_light()
{
    fm_assert(_light_center && _count != (size_t)-1);
    flush_vertexes(DrawLightmapMode);
    _light_center = {};
    _indexes[0] = quad_indexes(0);
    _quads[0] = {{
        {  1, -1 }, /* 3--1  1 */
        {  1,  1 }, /* | /  /| */
        { -1, -1 }, /* |/  / | */
        { -1,  1 }, /* 2  2--0 */
    }};

    using BF = Magnum::GL::Renderer::BlendFunction;

    GL::Renderer::setBlendFunction(BF::One, BF::One);
    framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{1});
    _count = 1; flush_vertexes(BlendLightmapMode);
    framebuffer.fb.mapForDraw(GL::Framebuffer::ColorAttachment{0});
    GL::Renderer::setBlendFunction(BF::SourceAlpha, BF::OneMinusSourceAlpha);
}

GL::Texture2D& lightmap_shader::scratch_texture()
{
    fm_assert(_count == (size_t)-1);
    fm_debug_assert(framebuffer.scratch.id());
    return framebuffer.scratch;
}

GL::Texture2D& lightmap_shader::accum_texture()
{
    fm_assert(_count == (size_t)-1);
    fm_debug_assert(framebuffer.accum.id());
    return framebuffer.accum;
}

bool light_s::operator==(const light_s&) const noexcept = default;

lightmap_shader::~lightmap_shader() = default;


} // namespace floormat
