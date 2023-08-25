#include "shaders/lightmap.hpp"
#include "compat/assert.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "src/chunk.hpp"
#include "src/tile-bbox.hpp"
#include "src/tile-atlas.hpp"
#include "src/entity.hpp"
#include <utility>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/Iterable.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>

#if defined __CLION_IDE__ || defined __clang__
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

namespace floormat {

namespace {

template<typename T>
constexpr T poor_mans_ceil(T x)
{
    static_assert(std::is_floating_point_v<T>);
    const auto x0 = (int64_t)x;
    if (x > x0)
        return T(x0 + (int64_t)1);
    else
        return x0;
}

constexpr auto neighbor_count = 4;
constexpr float fuzz_pixels = 16;
constexpr float shadow_wall_depth = 8;
constexpr float real_image_size = 1024;

constexpr auto half_neighbors = (int)poor_mans_ceil(neighbor_count/2.f);

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
    mesh.addVertexBuffer(std::forward<T>(vert), 0, lightmap_shader::Position{})
        .setIndexBuffer(std::forward<U>(index), 0, GL::MeshIndexType::UnsignedShort)
        .setCount(6);
    return mesh;
}

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

    //framebuffer.depth = GL::Renderbuffer{};
    //framebuffer.depth.setStorage(GL::RenderbufferFormat::DepthComponent32F, size);

    framebuffer.fb = GL::Framebuffer{{ {}, size }};
    framebuffer.fb
        //.attachRenderbuffer(GL::Framebuffer::BufferAttachment::Depth, framebuffer.depth);
        .attachTexture(GL::Framebuffer::ColorAttachment{0}, framebuffer.scratch, 0)
        .attachTexture(GL::Framebuffer::ColorAttachment{1}, framebuffer.accum, 0)
        //.clearDepth(0);
        .clearColor(0, Color4{1, 0, 1, 1})
        .clearColor(1, Color4{0, 0, 0, 1});

    return framebuffer;
}

GL::Mesh lightmap_shader::make_occlusion_mesh()
{
    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(vertex_buf, 0, Position{})
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

std::array<Vector3, 4>& lightmap_shader::alloc_rect()
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
        auto vertexes_ = std::move(vertexes);
        auto indexes_ = std::move(indexes);
        vertexes = Array<std::array<Vector3, 4>>{ValueInit, capacity};
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

lightmap_shader::lightmap_shader()
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

    auto blend_vertexes = std::array<Vector3, 4>{{
        {  1, -1, 0 }, /* 3--1  1 */
        {  1,  1, 0 }, /* | /  /| */
        { -1, -1, 0 }, /* |/  / | */
        { -1,  1, 0 }, /* 2  2--0 */
    }};
    light_mesh = make_light_mesh(GL::Buffer{blend_vertexes}, GL::Buffer{quad_indexes(0)});

    framebuffer.scratch.bind(TextureSampler);
    setUniform(SamplerUniform, TextureSampler);
    setUniform(LightColorUniform, Color3{1, 1, 1});
    setUniform(SizeUniform, Vector2(1));
    setUniform(CenterFragcoordUniform, Vector2(0, 0));
    setUniform(CenterClipUniform, Vector2(-1, -1));
    setUniform(RangeUniform, 1.f);
    setUniform(ModeUniform, DrawLightmapMode);
    setUniform(FalloffUniform, (uint32_t)light_falloff::constant);
}

std::array<UnsignedShort, 6> lightmap_shader::quad_indexes(size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

void lightmap_shader::add_light(Vector2 neighbor_offset, const light_s& light)
{
    neighbor_offset += Vector2((float)half_neighbors);

    constexpr auto tile_size = TILE_SIZE2.sum()/2;
    float range;

    switch (light.falloff)
    {
    default:
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

    auto center_fragcoord = light.center + neighbor_offset * chunk_size + chunk_offset; // window-relative coordinates
    auto center_clip = clip_start + center_fragcoord * clip_scale; // clip coordinates

    float alpha = light.color.a() / 255.f;
    auto color = (Vector3{light.color.rgb()} / 255.f) * alpha;

    framebuffer.fb.mapForDraw({
        { 0u, GL::Framebuffer::ColorAttachment{0} },
    });

    setUniform(LightColorUniform, color * alpha);
    setUniform(SizeUniform, Vector2(1) / real_image_size);
    setUniform(CenterFragcoordUniform, center_fragcoord * image_size_ratio);
    setUniform(CenterClipUniform, center_clip);
    setUniform(RangeUniform, range * image_size_ratio.sum()/2);
    setUniform(FalloffUniform, (uint32_t)light.falloff);

    setUniform(ModeUniform, DrawLightmapMode);
    AbstractShaderProgram::draw(light_mesh);

    setUniform(ModeUniform, DrawShadowsMode);
    setUniform(LightColorUniform, Color3{0, 0, 0});
    fm_assert(occlusion_mesh.id());
    auto mesh_view = GL::MeshView{occlusion_mesh};
    mesh_view.setCount((int32_t)count*6);
    AbstractShaderProgram::draw(mesh_view);

    framebuffer.fb.mapForDraw({
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
    GL::Renderer::setBlendFunction(0, BlendFunction::One, BlendFunction::Zero);
    GL::Renderer::setBlendFunction(1, BlendFunction::One, BlendFunction::One);
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

void lightmap_shader::add_rect(Vector2 neighbor_offset, Vector2 min, Vector2 max)
{
    auto off = neighbor_offset*chunk_size + chunk_offset;
    min += off;
    max += off;

    const auto vertexes = std::array<Vector3, 4>{{
        { max.x(), min.y(), 0 },
        { max.x(), max.y(), 0 },
        { min.x(), min.y(), 0 },
        { min.x(), max.y(), 0 },
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
        auto s1 = vertexes[src1], s2 = vertexes[src2];
        verts[dest1] = Vector3(s1.x(), s1.y(), 1);
        verts[dest2] = Vector3(s2.x(), s2.y(), 1);
        constexpr auto scale = Vector3(clip_scale, 1);
        constexpr auto start = Vector3(clip_start, 0);
        for (auto& x : verts)
            x = start + x * scale;
        for (auto i = 0uz; i < 4; i++)
            alloc_rect() = verts;
    }
}

void lightmap_shader::add_rect(Vector2 neighbor_offset, Pair<Vector2, Vector2> minmax)
{
    auto [min, max] = minmax;
    add_rect(neighbor_offset, min, max);
}

void lightmap_shader::add_chunk(Vector2 neighbor_offset, chunk& c)
{
    neighbor_offset += Vector2(half_neighbors);

    add_geometry(neighbor_offset, c);
    add_entities(neighbor_offset, c);
}

void lightmap_shader::add_geometry(Vector2 neighbor_offset, chunk& c)
{
    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        auto t = c[i];
        if (auto atlas = t.ground_atlas())
            if (atlas->pass_mode(pass_mode::pass) == pass_mode::blocked)
            {
                auto [min, max] = whole_tile(i);
                constexpr auto fuzz = Vector2(fuzz_pixels, fuzz_pixels), fuzz2 = fuzz*2;
                add_rect(neighbor_offset, {min-fuzz, max+fuzz2});
            }
        if (auto atlas = t.wall_north_atlas())
            if (atlas->pass_mode(pass_mode::blocked) == pass_mode::blocked)
            {
                auto start = tile_start(i);
                auto min = start - Vector2(0, shadow_wall_depth),
                     max = start + Vector2(TILE_SIZE2[0], 0);
                constexpr auto fuzz = Vector2(fuzz_pixels, 0), fuzz2 = fuzz*2;
                add_rect(neighbor_offset, {min-fuzz, max+fuzz2});
            }
        if (auto atlas = t.wall_west_atlas())
            if (atlas->pass_mode(pass_mode::blocked) == pass_mode::blocked)
            {
                auto start = tile_start(i);
                auto min = start - Vector2(shadow_wall_depth, 0),
                     max = start + Vector2(0, TILE_SIZE[1]);
                constexpr auto fuzz = Vector2(0, fuzz_pixels), fuzz2 = fuzz*2;
                add_rect(neighbor_offset, {min-fuzz, max+fuzz2});
            }
    }
}

void lightmap_shader::add_entities(Vector2 neighbor_offset, chunk& c)
{
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

bool light_s::operator==(const light_s&) const noexcept = default;

lightmap_shader::~lightmap_shader() = default;

} // namespace floormat
