#include "shaders/lightmap.hpp"
#include "compat/assert.hpp"
#include "src/tile-defs.hpp"
#include "loader/loader.hpp"
#include "src/chunk.hpp"
#include <Corrade/Containers/Iterable.h>
#include <cmath>
#include <Magnum/Magnum.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
//#include "src/tile-bbox.hpp"

#if defined __CLION_IDE__ || defined __clang__
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

namespace floormat {

namespace {

constexpr auto chunk_size   = TILE_SIZE2 * TILE_MAX_DIM;
constexpr auto chunk_offset = TILE_SIZE2/2;
constexpr auto image_size = iTILE_SIZE2 * TILE_MAX_DIM;
constexpr auto buffer_size = 256uz;

} // namespace

auto lightmap_shader::make_framebuffer(Vector2i size) -> Framebuffer
{
    Framebuffer framebuffer;

    framebuffer.fb = GL::Framebuffer{{ {}, size }};

    framebuffer.color = GL::Texture2D{};
    framebuffer.color.setStorage(1, GL::TextureFormat::RGB8, size);
    //framebuffer.depth = GL::Renderbuffer{};
    //framebuffer.depth.setStorage(GL::RenderbufferFormat::DepthComponent32F, fb_size);

    framebuffer.fb.attachTexture(GL::Framebuffer::ColorAttachment{0}, framebuffer.color, 0);
    //framebuffer.fb.attachRenderbuffer(GL::Framebuffer::BufferAttachment::Depth, framebuffer.depth);
    framebuffer.fb.clearColor(0, Color4{0.f, 0.f, 0.f, 1.f});
    //framebuffer.fb.clearDepth(0);

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
    accum { make_framebuffer(image_size) },
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
}

void lightmap_shader::flush_vertexes()
{
    if (_count > 0)
    {
        _index_buf.setSubData(0, ArrayView<std::array<UnsignedShort, 6>>{_indexes, 1});
        _vertex_buf.setSubData(0, ArrayView<std::array<Vector2, 4>>{_quads, 1});

        GL::MeshView mesh{_mesh};
        mesh.setCount((int)(6 * _count));
        mesh.setIndexRange(0, 0, (uint32_t)(_count * 6 - 1));
        AbstractShaderProgram::draw(mesh);
        _count = 0;
    }
}

std::array<UnsignedShort, 6> lightmap_shader::quad_indexes(size_t N)
{
    using u16 = UnsignedShort;
    return {                                        /* 3--1  1 */
        (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
        (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
    };                                              /* 2  2--0 */
}

void lightmap_shader::add_light(Vector2i neighbor_offset, const light_s& light)
{
    fm_debug_assert(_count == 0);
    fm_debug_assert(_quads.size() > 0);

    constexpr auto tile_size = TILE_SIZE2.sum()/2;
    constexpr auto scale = 2/chunk_size;
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
    auto center_clip = Vector2{center} * scale; // clip coordinate
    constexpr auto image_size_factor = Vector2(image_size) / Vector2(chunk_size);
    auto center_fragcoord = center * image_size_factor; // window-relative coordinates

    _indexes[_count] = quad_indexes(0);
    _quads[_count] = std::array<Vector2, 4>{{
        {  I_clip + center_clip.x(), -I_clip + center_clip.y() },
        {  I_clip + center_clip.x(),  I_clip + center_clip.y() },
        { -I_clip + center_clip.x(), -I_clip + center_clip.y() },
        { -I_clip + center_clip.x(),  I_clip + center_clip.y() },
    }};

    _count++;

    float alpha = light.color.a() / 255.f;
    auto color = Vector3{light.color.rgb()} / 255.f;

    setUniform(ColorIntensityUniform, Vector4{Vector3{color} * alpha, I });
    setUniform(CenterUniform, center_fragcoord);
    setUniform(FalloffUniform, (uint32_t)light.falloff);
    setUniform(SizeUniform, 1.f/image_size_factor);

    _light_center = center;
    flush_vertexes();

    setUniform(ColorIntensityUniform, Vector4{0, 0, 0, 1});
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

void lightmap_shader::add_chunk(Vector2i neighbor_offset, const chunk& c)
{
    fm_assert(_light_center);

    for (const auto& e_ : c.entities())
    {
        const auto& e = *e_;
        if (e.is_virtual())
            continue;
        if (e.pass == pass_mode::pass || e.pass == pass_mode::see_through)
            continue;
        auto li = *_light_center;
        auto center = Vector2(e.offset) + Vector2(e.bbox_offset) +
                      Vector2(e.coord.local()) * TILE_SIZE2 +
                      Vector2(neighbor_offset)*chunk_size + chunk_offset;
        const auto x = center.x(), y = center.y(),
                   w = (float)e.bbox_size.x(), h = (float)e.bbox_size.y();
        /* 3--1  1 */
        /* | /  /| */
        /* |/  / | */
        /* 2  2--0 */
        const auto vertexes = std::array<Vector2, 4>{{
            {  w + x, -h + y }, // bottom right
            {  w + x,  h + y }, // top right
            { -w + x, -h + y }, // bottom left
            { -w + x,  h + y }, // top left
        }};
        struct pair { uint8_t first, second; };
        constexpr std::array<pair, 4> from = {{
            { 3, 1 }, // side #1: 3 -> 2, 1 -> 0
            { 1, 0 }, // side #2: 1 -> 3, 0 -> 2
            { 0, 2 }, // side #3: 0 -> 1, 2 -> 3
            { 2, 3 }, // side #4: 2 -> 0, 3 -> 1
        }};
        constexpr std::array<pair, 4> to = {{
            { 2, 0 },
            { 3, 2 },
            { 1, 3 },
            { 0, 1 },
        }};
        for (auto i = 0uz; i < 4; i++)
        {
            auto [src1, src2] = from[i];
            auto [dest1, dest2] = to[i];
            auto verts = vertexes;
            verts[dest1] = project_vertex(li, vertexes[src1], {128, 128});
            verts[dest2] = project_vertex(li, vertexes[src2], {128, 128});
            // todo
        }
    }
}

void lightmap_shader::add_quad(const std::array<Vector2, 4>& quad)
{
    fm_debug_assert(_count < buffer_size);
    const auto i = _count++;
    _quads[i] = quad;
    _indexes[i] = quad_indexes(i);
    if (i+1 == buffer_size) [[unlikely]]
        flush_vertexes();
}

void lightmap_shader::clear()
{
    _light_center = {};
    framebuffer.fb.clearColor(0, Vector4ui{0});
    accum.fb.clearColor(0, Vector4ui{0});
    //framebuffer.fb.clearDepth(0);
}

void lightmap_shader::bind()
{
    framebuffer.fb.bind();
}

void lightmap_shader::begin(Vector2i neighbor_offset, const light_s& light)
{
    fm_assert(_count == 0 && !_light_center);
    clear();
    bind();
    add_light(neighbor_offset, light);
    flush_vertexes();
}

void lightmap_shader::end()
{
    fm_assert(_light_center);
    flush_vertexes();
    _light_center = {};
}

GL::Texture2D& lightmap_shader::texture()
{
    fm_assert(_count == 0);
    fm_debug_assert(framebuffer.color.id());
    return framebuffer.color;
}

bool light_s::operator==(const light_s&) const noexcept = default;

lightmap_shader::~lightmap_shader() = default;

} // namespace floormat
