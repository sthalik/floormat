#include "shaders/lightmap.hpp"
#include "compat/assert.hpp"
#include "src/tile-defs.hpp"
#include "src/chunk.hpp"
#include "src/tile-bbox.hpp"
#include "src/ground-atlas.hpp"
#include "src/wall-atlas.hpp"
#include "src/quads.hpp"
#include "src/object.hpp"
#include "loader/loader.hpp"
#include <utility>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/Iterable.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

using namespace floormat::Quads;

namespace {

constexpr auto neighbor_count = 4;
constexpr float shadow_wall_depth = 8;
constexpr float real_image_size = 1024;

constexpr auto half_neighbors = (int)Math::ceil(neighbor_count/2.f);

constexpr auto image_size   = TILE_SIZE2 * TILE_MAX_DIM * neighbor_count;
constexpr auto chunk_size   = TILE_SIZE2 * TILE_MAX_DIM;
constexpr auto chunk_offset = TILE_SIZE2/2;

constexpr auto clip_start = Vector2{-1, -1};
constexpr auto clip_scale = 2/image_size;

constexpr auto image_size_ratio = real_image_size / image_size;

template<typename T, typename U>
GL::Mesh make_light_mesh(T&& vert, U&& index)
{
    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(forward<T>(vert), 0, lightmap_shader::Position{})
        .setIndexBuffer(forward<U>(index), 0, GL::MeshIndexType::UnsignedShort)
        .setCount(6);
    return mesh;
}

struct Block final
{
    Vector4 light_color;
    Vector2 scale;
    Vector2 center_fragcoord;
    Vector2 center_clip;
    float range;
    uint32_t falloff;
    float radius;
    char _pad0[12];
};

} // namespace

auto lightmap_shader::make_framebuffer(Vector2i size) -> Framebuffer
{
    Framebuffer framebuffer;

    framebuffer.scratch = GL::Texture2D{};
    framebuffer.scratch
        .setWrapping(GL::SamplerWrapping::ClampToBorder)
        .setBorderColor(Color4{0, 0, 0, 1})
        .setStorage(1, GL::TextureFormat::RGBA8, size)
        .setMagnificationFilter(GL::SamplerFilter::Nearest);

    framebuffer.accum = GL::Texture2D{};
    framebuffer.accum
        .setWrapping(GL::SamplerWrapping::ClampToBorder)
        .setBorderColor(Color4{0, 0, 0, 1})
        .setStorage(1, GL::TextureFormat::RGBA8, size)
        .setMagnificationFilter(GL::SamplerFilter::Nearest);

    framebuffer.fb = GL::Framebuffer{{ {}, size }};
    framebuffer.fb
        .attachTexture(GL::Framebuffer::ColorAttachment{0}, framebuffer.scratch, 0)
        .attachTexture(GL::Framebuffer::ColorAttachment{1}, framebuffer.accum, 0)
        .clearColor(0, Color4{0, 0, 0, 1})
        .clearColor(1, Color4{0, 0, 0, 1});

    return framebuffer;
}

GL::Mesh lightmap_shader::make_occlusion_mesh()
{
    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(vertex_buf, 0, Segment{}, ShadowCoord{})
        .setIndexBuffer(index_buf, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * capacity));
    return mesh;
}

void lightmap_shader::begin_occlusion()
{
    count = 0;
}

void lightmap_shader::end_occlusion()
{
    bool create_mesh = !vertex_buf.id();

    if (create_mesh)
    {
        occlusion_mesh = GL::Mesh{NoCreate};
        vertex_buf = GL::Buffer{vertexes.prefix(capacity), GL::BufferUsage::DynamicDraw};
        index_buf = GL::Buffer{indexes.prefix(capacity)};
        occlusion_mesh = make_occlusion_mesh();
    }
    else
    {
        if (!occlusion_mesh.id())
            occlusion_mesh = make_occlusion_mesh();
        if (count > 0)
        {
            vertex_buf.setSubData(0, vertexes.prefix(count));
            index_buf.setSubData(0, indexes.prefix(count));
        }
    }
}

std::array<lightmap_shader::shadow_vertex, 4>& lightmap_shader::alloc_quad()
{
    if (count == capacity)
    {
        if (capacity == 0)
            capacity = starting_capacity;
        else
            capacity <<= 1;
        fm_debug_assert(count < capacity);

        occlusion_mesh = GL::Mesh{NoCreate};
        vertex_buf = GL::Buffer{NoCreate};
        index_buf = GL::Buffer{NoCreate};
        auto vertexes_ = move(vertexes);
        auto indexes_ = move(indexes);
        vertexes = Array<std::array<shadow_vertex, 4>>{ValueInit, capacity};
        indexes = Array<std::array<UnsignedShort, 6>>{ValueInit, capacity};
        for (auto i = 0uz; i < count; i++)
            vertexes[i] = vertexes_[i];
        for (auto i = 0uz; i < count; i++)
            indexes[i] = indexes_[i];
        indexes[count] = quad_indexes(count);
        auto& ret = vertexes[count];
        count++;
        return ret;
    }
    else
    {
        fm_debug_assert(count < capacity);
        auto& ret = vertexes[count];
        indexes[count] = quad_indexes(count);
        count++;
        return ret;
    }
}

lightmap_shader::lightmap_shader(texture_unit_cache& tuc) : tuc{tuc}
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

    framebuffer = make_framebuffer(Vector2i((int)real_image_size));

    auto blend_vertexes = quad {{
        {  1, -1, 0 }, /* 3--1  1 */
        {  1,  1, 0 }, /* | /  /| */
        { -1, -1, 0 }, /* |/  / | */
        { -1,  1, 0 }, /* 2  2--0 */
    }};
    light_mesh = make_light_mesh(GL::Buffer{blend_vertexes}, GL::Buffer{quad_indexes(0)});

    setUniform(SamplerUniform, 0);
    setUniform(ModeUniform, DrawShadowsMode);

    Block block {
        .light_color = 0xffffffff_rgbaf,
        .scale = Vector2(1),
        .center_fragcoord = Vector2(0, 0),
        .center_clip = Vector2(-1, -1),
        .range = 1.f,
        .falloff = (uint32_t)light_falloff::constant,
        .radius = 0.f,
        ._pad0 = {},
    };

    setUniformBlockBinding(uniformBlockIndex("Lightmap"_s), BlockUniform);
    block_uniform_buf.setData({&block, 1});
    block_uniform_buf.bind(GL::Buffer::Target::Uniform, BlockUniform);
}

void lightmap_shader::add_light(Vector2 neighbor_offset, const light_s& light)
{
    neighbor_offset += Vector2((float)half_neighbors);

    constexpr auto tile_size = TILE_SIZE2.sum()/2;
    float range = 0;

    fm_assert(light.falloff < light_falloff::COUNT);
    switch (light.falloff)
    {
    case light_falloff::COUNT: std::unreachable();
    case light_falloff::constant:
        range = TILE_MAX_DIM;
        break;
    case light_falloff::linear:
    case light_falloff::quadratic:
        range = light.dist;
        break;
    }

    range *= tile_size;
    range = std::fmax(0.f, range);

    auto center_fragcoord = light.center + neighbor_offset * chunk_size + chunk_offset;
    auto center_clip = clip_start + center_fragcoord * clip_scale;

    // light radius in pixels

    Block block = {
        .light_color = Vector4(light.color)  / 255.f,
        .scale = Vector2(1) / real_image_size,
        .center_fragcoord = center_fragcoord * image_size_ratio,
        .center_clip = center_clip,
        .range = range * image_size_ratio.sum()/2,
        .falloff = (uint32_t)light.falloff,
        .radius = light.radius * image_size_ratio.sum()/2,
        ._pad0 = {},
    };

    block_uniform_buf.setSubData(0, {&block, 1});

    // --- Pass 1: accumulate shadow mask to attachment 0 ---
    framebuffer.fb.mapForDraw({
        { 0u, GL::Framebuffer::ColorAttachment{0} },
        { 1u, GL::Framebuffer::DrawAttachment::None },
    });

    // clear shadow mask to black (no shadow)
    framebuffer.fb.clearColor(0, Color4{0, 0, 0, 1});

    using BlendFunction = Magnum::GL::Renderer::BlendFunction;
    GL::Renderer::setBlendFunction(0, BlendFunction::One, BlendFunction::One);

    setUniform(ModeUniform, DrawShadowsMode);
    fm_assert(occlusion_mesh.id());
    auto mesh_view = GL::MeshView{occlusion_mesh};
    mesh_view.setCount((int32_t)count*6);
    AbstractShaderProgram::draw(mesh_view);

    // --- Pass 2: compute light * (1 - shadow), accumulate to attachment 1 ---
    framebuffer.fb.mapForDraw({
        { 0u, GL::Framebuffer::DrawAttachment::None },
        { 1u, GL::Framebuffer::ColorAttachment{1} },
    });

    setUniform(ModeUniform, BlendLightmapMode);
    AbstractShaderProgram::draw(light_mesh);
}

void lightmap_shader::bind()
{
    framebuffer.fb.bind();
    GL::Renderer::setScissor({{}, Vector2i(image_size)});
    framebuffer.fb.clearColor(1, Color4{0, 0, 0, 1});
    using BlendFunction = Magnum::GL::Renderer::BlendFunction;
    GL::Renderer::setBlendFunction(0, BlendFunction::One, BlendFunction::One);
    GL::Renderer::setBlendFunction(1, BlendFunction::One, BlendFunction::One);
    setUniform(SamplerUniform, tuc.bind(framebuffer.scratch));
}

void lightmap_shader::finish() // NOLINT(*-convert-member-functions-to-static)
{
    using BlendFunction = Magnum::GL::Renderer::BlendFunction;
    GL::Renderer::setBlendFunction(BlendFunction::SourceAlpha, BlendFunction::OneMinusSourceAlpha);
}

GL::Texture2D& lightmap_shader::accum_texture()
{
    fm_debug_assert(framebuffer.accum.id());
    return framebuffer.accum;
}

int lightmap_shader::iter_bounds()
{
    return half_neighbors;
}

void lightmap_shader::add_segment(Vector2 neighbor_offset, Vector2 endpoint_a, Vector2 endpoint_b)
{
    auto off = neighbor_offset*chunk_size + chunk_offset;
    endpoint_a += off;
    endpoint_b += off;

    // convert to clip space for the segment endpoints
    auto a_clip = clip_start + endpoint_a * clip_scale;
    auto b_clip = clip_start + endpoint_b * clip_scale;

    auto seg = Vector4{a_clip.x(), a_clip.y(), b_clip.x(), b_clip.y()};

    auto& verts = alloc_quad();
    // shadow_coord.x = endpoint select (0 = A, 1 = B)
    // shadow_coord.y = near/far (0 = far/projected, 1 = near/at endpoint)
    verts[0] = { seg, Vector2{0, 0} };  // endpoint A, far
    verts[1] = { seg, Vector2{1, 0} };  // endpoint B, far
    verts[2] = { seg, Vector2{0, 1} };  // endpoint A, near
    verts[3] = { seg, Vector2{1, 1} };  // endpoint B, near
}

void lightmap_shader::add_chunk(Vector2 neighbor_offset, chunk& c)
{
    neighbor_offset += Vector2(half_neighbors);

    add_geometry(neighbor_offset, c);
    add_objects(neighbor_offset, c);
}

void lightmap_shader::add_geometry(Vector2 neighbor_offset, chunk& c)
{
    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        auto t = c[i];
        if (auto atlas = t.ground_atlas())
            if (atlas->pass_mode() == pass_mode::blocked)
            {
                auto [min, max] = whole_tile(i);
                // 4 edges: N, E, S, W
                add_segment(neighbor_offset, {min.x(), max.y()}, {max.x(), max.y()}); // north edge
                add_segment(neighbor_offset, {max.x(), max.y()}, {max.x(), min.y()}); // east edge
                add_segment(neighbor_offset, {max.x(), min.y()}, {min.x(), min.y()}); // south edge
                add_segment(neighbor_offset, {min.x(), min.y()}, {min.x(), max.y()}); // west edge
            }
        if (auto atlas = t.wall_north_atlas())
            if (atlas->info().passability == pass_mode::blocked)
            {
                auto start = tile_start(i);
                auto min = start - Vector2(0, shadow_wall_depth),
                     max = start + Vector2(TILE_SIZE2[0], 0);
                add_segment(neighbor_offset, {min.x(), max.y()}, {max.x(), max.y()}); // north edge
                add_segment(neighbor_offset, {max.x(), max.y()}, {max.x(), min.y()}); // east edge
                add_segment(neighbor_offset, {max.x(), min.y()}, {min.x(), min.y()}); // south edge
                add_segment(neighbor_offset, {min.x(), min.y()}, {min.x(), max.y()}); // west edge
            }
        if (auto atlas = t.wall_west_atlas())
            if (atlas->info().passability == pass_mode::blocked)
            {
                auto start = tile_start(i);
                auto min = start - Vector2(shadow_wall_depth, 0),
                     max = start + Vector2(0, TILE_SIZE[1]);
                add_segment(neighbor_offset, {min.x(), max.y()}, {max.x(), max.y()}); // north edge
                add_segment(neighbor_offset, {max.x(), max.y()}, {max.x(), min.y()}); // east edge
                add_segment(neighbor_offset, {max.x(), min.y()}, {min.x(), min.y()}); // south edge
                add_segment(neighbor_offset, {min.x(), min.y()}, {min.x(), max.y()}); // west edge
            }
    }
}

void lightmap_shader::add_objects(Vector2 neighbor_offset, chunk& c)
{
    for (const auto& eʹ : c.objects())
    {
        const auto& e = *eʹ;
        if (e.is_virtual())
            continue;
        if (e.pass == pass_mode::pass || e.pass == pass_mode::see_through)
            continue;
        auto center = Vector2(e.offset) + Vector2(e.bbox_offset) +
                      Vector2(e.coord.local()) * TILE_SIZE2;
        auto half = Vector2(e.bbox_size)*.5f;
        auto min = center - half, max = center + half;

        add_segment(neighbor_offset, {min.x(), max.y()}, {max.x(), max.y()}); // north
        add_segment(neighbor_offset, {max.x(), max.y()}, {max.x(), min.y()}); // east
        add_segment(neighbor_offset, {max.x(), min.y()}, {min.x(), min.y()}); // south
        add_segment(neighbor_offset, {min.x(), min.y()}, {min.x(), max.y()}); // west
    }
}

bool light_s::operator==(const light_s&) const noexcept = default;

lightmap_shader::~lightmap_shader() = default;

} // namespace floormat
