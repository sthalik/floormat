#include "app.hpp"
#include "src/point.inl"
#include "src/depth.hpp"
#include "src/world.hpp"
#include "src/scenery-proto.hpp"
#include "shaders/shader.hpp"
#include "shaders/lightmap.hpp"
#include "shaders/texture-unit-cache.hpp"
#include "loader/loader.hpp"
#include "compat/borrowed-ptr.inl"
#include <cr/Pair.h>
#include <cr/StridedArrayView.h>
#include <mg/Color.h>
#include <mg/DefaultFramebuffer.h>
#include <mg/Framebuffer.h>
#include <mg/Image.h>
#include <mg/Mesh.h>
#include <mg/PixelFormat.h>
#include <mg/Renderbuffer.h>
#include <mg/RenderbufferFormat.h>
#include <mg/Renderer.h>
#include <mg/Shader.h>
#include <mg/Texture.h>
#include <mg/TextureArray.h>
#include <mg/TextureFormat.h>

namespace floormat::Test {
namespace {

u32 value_at(point p)
{
    return Depth::value_at(p, 0);
}

void test_point()
{
    {
        constexpr auto my_chunk = chunk_size<int> * Vector3i{1, 2, 0};
        constexpr auto my_tile  = iTILE_SIZE * Vector3i{4, 5, 0};
        constexpr auto my_pixel = my_chunk + my_tile + Vector3i{6, 7, 0};

        constexpr auto a = point {{1, 2, 0}, {4, 5}, {6, 7}};
        const auto Va = value_at(a);
        fm_assert(Vector3i(a) == my_pixel);

        constexpr auto b = point {{1, 2, -1}, {4, 5}, {6, 7}};

        const auto Vb = value_at(b);
        fm_assert(Va > Vb);

        constexpr auto c = point {{1, 2, -1}, {4, 5}, {3, 10}};
        const auto Vc = value_at(c);
        fm_assert(Vc == Vb);
        fm_assert_equal(Vector3i{1  * chunk_size<int> + 4 * tile_size_xy + 3,
                                 2  * chunk_size<int> + 5 * tile_size_xy + 10,
                                -1  * tile_size_z},
                        Vector3i(c));

        constexpr auto d = point {{1, 2, 0}, {4, 5}, {6, 5}};
        const auto Vd = value_at(d);
        fm_assert(Vd < Va);
        fm_assert(Vd > Vb);

        constexpr auto e = point {{1, 2, -1}, {4, 5}, {6, 5}};
        const auto Ve = value_at(e);
        fm_assert(Ve < Vb);
    }

    {
        constexpr auto a = point {{1, 2, 0}, {1, 2}, {}};
        const auto Va = value_at(a);
        constexpr auto b = point {{2, 1, 0}, {2, 1}, {}};
        const auto Vb = value_at(b);
        fm_assert(Va == Vb);
        constexpr auto c = point {{2, 1, 0}, {2, 1}, {1, 0}};
        const auto Vc = value_at(c);
        fm_assert(Vc > Vb);
        fm_assert(Va < Vc);
        constexpr auto d = point {{2, 1, 0}, {1, 1}, {}};
        const auto Vd = value_at(d);
        fm_assert(Vd < Vc);
        constexpr auto e = point {{2, 1, 0}, {1, 1}, {1, 0}};
        const auto Ve = value_at(e);
        fm_assert(Ve > Vd);
        fm_assert(Ve < Vc);
    }
    {
        constexpr auto a = point{{11, 22, 0}, {4, 5}, {6, 7}};
        constexpr auto b = point{{22, 11, 0}, {4, 5}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(b));
        constexpr auto c = point{{22, 11, 0}, {5, 4}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(c));
        constexpr auto d = point{{11, 22, 0}, {4, 5}, {7, 6}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(d));
        constexpr auto e = point{{13, 20, 0}, {4, 5}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(e));
        constexpr auto f = point{{13, 20, 0}, {2, 7}, {6, 7}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(f));
        constexpr auto g = point{{13, 20, 0}, {2, 7}, {8, 5}};
        fm_assert_equal(Depth::value_atʹ(a), Depth::value_atʹ(g));
    }

    {
        constexpr auto p = point{{7, -11, 3}, {5, 9}, {13, -17}};
        static_assert(Vector3i(p) == Vector3i{
            chunk_size<int> * 7 + tile_size_xy * 5 + 13,
            chunk_size<int> * -11 + tile_size_xy * 9 - 17,
            tile_size_z * 3,
        });
    }
}

void test_shader_program()
{
    texture_unit_cache tuc;
    tile_shader S{tuc};
    // the only place lightmap_shader's GLSL gets compiled outside the editor
    lightmap_shader L{tuc};
}

void test_tuc_stale_binding()
{
    texture_unit_cache tuc;

    auto make_tex = [] {
        auto tex = GL::Texture2D{};
        tex.setStorage(1, GL::TextureFormat::RGBA8, Vector2i{1, 1});
        return tex;
    };

    auto tex = make_tex();
    const auto unit = tuc.bind(tex);
    fm_assert_equal(1uz, tuc.stats().cache_miss);

    fm_assert_equal(unit, tuc.bind(tex));
    fm_assert_equal(1uz, tuc.stats().cache_hit);
    fm_assert_equal(1uz, tuc.stats().cache_miss);

    // the grow_texture pattern from sprite-atlas.cpp: move-assigning into the
    // same wrapper keeps the address and swaps the GL name
    tex = make_tex();
    fm_assert_equal(unit, tuc.bind(tex));
    fm_assert_equal(2uz, tuc.stats().cache_miss);
}

void test_tuc_rebinds_reused_texture_name()
{
    texture_unit_cache tuc;
    auto tex = GL::Texture2D{};
    tex.setStorage(1, GL::TextureFormat::RGBA8, Vector2i{1, 1});
    const auto unit = tuc.bind(tex);
    tex = GL::Texture2D{NoCreate};
    tex = GL::Texture2D{};
    tex.setStorage(1, GL::TextureFormat::RGBA8, Vector2i{1, 1});
    fm_assert_equal(unit, tuc.bind(tex));

    GLint active = 0, bound = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    glActiveTexture((GLenum)active);
    fm_assert_equal(tex.id(), (GLuint)bound);
}

void test_shader_lightmap_binding()
{
    texture_unit_cache tuc;
    tile_shader S{tuc};
    GL::Texture2DArray atlas;
    atlas.setStorage(1, GL::TextureFormat::RGBA8, Vector3i{1});
    GL::Texture2D lightmap;
    lightmap.setStorage(1, GL::TextureFormat::RGBA8, Vector2i{1});
    GL::Mesh mesh;
    mesh.setCount(3);
    // A surfaceless EGL context has no default framebuffer to draw into.
    GL::Renderbuffer color;
    color.setStorage(GL::RenderbufferFormat::RGBA8, Vector2i{1});
    GL::Framebuffer framebuffer{{{}, Vector2i{1}}};
    framebuffer.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, color).bind();

    const auto draw = [&](const GL::Texture2D* expected = nullptr) {
        GL::Texture2D::unbind(0);
        S.draw(atlas, mesh);

        GLint program = 0, unit = -1, active = 0, texture = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetUniformiv((GLuint)program, glGetUniformLocation((GLuint)program, "lightmap_sampler"), &unit);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glActiveTexture((GLenum)active);
        fm_assert(texture != 0);
        if (expected)
            fm_assert_equal(expected->id(), (GLuint)texture);
        fm_assert((GL::Texture2D::wrap((GLuint)texture).imageSize(0) > Vector2i{}).all());
        fm_assert(unit != S.sampler());
        fm_assert_equal((GLenum)GL_NO_ERROR, glGetError());
    };

    draw();
    const auto stats = tuc.stats();
    draw();
    fm_assert_equal(stats.cache_miss, tuc.stats().cache_miss);
    S.set_lightmap(&lightmap);
    draw(&lightmap);
    S.set_lightmap(nullptr);
    draw();
    tuc.invalidate();
    draw();
    GL::defaultFramebuffer.bind();
}

Pair<Color4ub, Color4ub> lit_behind_object(lightmap_shader& L, pass_mode pass)
{
    constexpr auto light_pos = Vector2(4, 8) * TILE_SIZE2, behind = Vector2(12, 8) * TILE_SIZE2,
                   north = Vector2(4, 0) * TILE_SIZE2;

    auto w = world();
    auto proto = scenery_proto(loader.scenery("table0"));
    proto.offset = {};
    proto.bbox_offset = {};
    proto.bbox_size = Vector2ub(tile_size_xy/2);
    proto.pass = pass;
    w.make_scenery(w.make_id(), {{0, 0, 0}, {8, 8}}, move(proto));

    L.begin_occlusion();
    L.add_chunk({}, w[chunk_coords_{0, 0, 0}]);
    L.end_occlusion();
    L.bind();
    L.add_light({}, light_s{.center = light_pos, .color = Vector4ub{255}, .falloff = light_falloff::constant});
    L.finish();

    // same mapping as lightmap_shader::add_segment(): the chunk at offset 0 sits iter_bounds() chunks in
    const auto ns = (float)lightmap_shader::iter_bounds();
    const auto img = L.accum_texture().image(0, Image2D{PixelFormat::RGBA8Unorm});
    const auto pixels = img.pixels<Color4ub>();
    const auto texel_at = [&](Vector2 pos) {
        const auto t = Vector2i((pos + chunk_size<Vector2>*ns + half_tile<Vector2>) * Vector2(img.size()) / (chunk_size<Vector2>*2*ns));
        return pixels[(usize)t.y()][(usize)t.x()];
    };
    return { texel_at(behind), texel_at(north) };
}

void test_lightmap_object_shadows()
{
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::enable(GL::Renderer::Feature::Blending);

    texture_unit_cache tuc;
    lightmap_shader L{tuc};

    const auto [shadow, lit] = lit_behind_object(L, pass_mode::blocked);
    fm_assert_equal(Color4ub{255}, lit);
    fm_assert(shadow.r() < lit.r()/2);
    for (auto pass : { pass_mode::see_through, pass_mode::shoot_through, pass_mode::pass })
    {
        const auto [behind, north] = lit_behind_object(L, pass);
        fm_assert_equal(north, behind);
    }

    GL::Renderer::disable(GL::Renderer::Feature::Blending);
    GL::defaultFramebuffer.bind();
}

void test_tuc_eviction()
{
    texture_unit_cache tuc;
    constexpr int N = 400;
    fm_assert(GL::Shader::maxTextureImageUnits(GL::Shader::Type::Fragment) < N);
    GL::Texture2D tex[N];
    for (auto& t : tex)
        t.setStorage(1, GL::TextureFormat::RGBA8, Vector2i{1, 1});
    for (int round = 0; round < 3; round++)
        for (int j = 0; j < N; j++)
        {
            auto& t = tex[(j * 7 + round) % N];
            const auto u = tuc.bind(t);
            fm_assert(u > 0);
            fm_assert_equal(u, tuc.bind(t));
        }
    fm_assert(tuc.stats().cache_miss > (usize)N);
    fm_assert(tuc.stats().cache_hit >= 3*(usize)N);
}
} // namespace

void test_shader()
{
    test_point();
    test_shader_program();
    test_shader_lightmap_binding();
    test_tuc_stale_binding();
    test_tuc_rebinds_reused_texture_name();
    test_tuc_eviction();
    test_lightmap_object_shadows();
}

} // namespace floormat::Test
