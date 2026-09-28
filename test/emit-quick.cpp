#include "app.hpp"
#include "compat/array-size.hpp"
#include "src/spritebatch.hpp"
#include "src/anim-atlas.hpp"
#include "src/anim.hpp"
#include "src/scenery-proto.hpp"
#include "src/rotation.hpp"
#include "loader/loader.hpp"
#include "loader/anim-cell.hpp"
#include "loader/vobj-cell.hpp"
#include "shaders/shader.hpp"
#include "shaders/texture-unit-cache.hpp"
#include <cmath>
#include <cr/StridedArrayView.h>
#include <mg/Color.h>
#include <mg/Framebuffer.h>
#include <mg/Image.h>
#include <mg/PixelFormat.h>
#include <mg/Renderbuffer.h>
#include <mg/RenderbufferFormat.h>
#include <mg/Renderer.h>

namespace floormat::Test {

namespace {

constexpr Vector2i fb_size{512, 512};
constexpr Vector3 center{13, -7, 0};
// Blending is off, so the quad writes every pixel it covers, transparent texels included.
// A zero tint turns all of them into zeroes, which the clear color never is.
constexpr Color4 clear_color{1, 0, 1, 1};
constexpr Color4ub clear_color_ub{255, 0, 255, 255};
constexpr Color4ub zero{0, 0, 0, 0};

struct state
{
    texture_unit_cache tuc;
    tile_shader shader{tuc};
    SpriteBatch sb;
    GL::Renderbuffer color;
    GL::Framebuffer fb{{{}, fb_size}};
};

Image2D draw(state& s, const anim_atlas& atlas, rotation r, uint32_t frame, const Vector4& tint)
{
    s.fb.clearColor(0, clear_color);
    s.fb.bind();
    s.shader.set_tint(tint);
    s.sb.emit_quick(s.shader, atlas, r, frame, center, {1, 1, 1, 1});
    return s.fb.read({{}, fb_size}, Image2D{PixelFormat::RGBA8Unorm});
}

void test_quad(state& s, const anim_atlas& atlas, rotation r, uint32_t frame)
{
    const auto& g = atlas.group(r);
    const auto& f = atlas.frame(r, frame);

    // Window coordinates, y down, anchored like SpriteBatch::add_clickable.
    const auto p0 = Vector2(fb_size)*.5f + tile_shader::project(center + Vector3(g.offset)) - Vector2(f.ground);
    const auto p1 = p0 + Vector2(f.size);
    fm_assert(p0.x() >= 0 && p0.y() >= 0 && p1.x() <= (float)fb_size.x() && p1.y() <= (float)fb_size.y());

    // A pixel whose center lies on a half-pixel edge may go either way.
    const auto ox0 = (int)std::floor(p0.x()), ox1 = (int)std::ceil(p1.x()),
               oy0 = (int)std::floor(p0.y()), oy1 = (int)std::ceil(p1.y());
    const auto ix0 = (int)std::ceil(p0.x()), ix1 = (int)std::floor(p1.x()),
               iy0 = (int)std::ceil(p0.y()), iy1 = (int)std::floor(p1.y());

    {
        const auto img = draw(s, atlas, r, frame, {0, 0, 0, 0});
        const auto px = img.pixels<Color4ub>();
        for (auto row = 0; row < fb_size.y(); row++)
        {
            const auto y = fb_size.y() - 1 - row; // readback rows go bottom-up
            for (auto x = 0; x < fb_size.x(); x++)
            {
                const bool drawn = px[(uint32_t)row][(uint32_t)x] == zero;
                if (drawn && !(x >= ox0 && x < ox1 && y >= oy0 && y < oy1))
                    fm_abort("%s rotation %u frame %u: pixel (%d,%d) drawn outside the quad",
                             atlas.name().data(), (unsigned)r, frame, x, y);
                if (!drawn && x >= ix0 && x < ix1 && y >= iy0 && y < iy1)
                    fm_abort("%s rotation %u frame %u: pixel (%d,%d) inside the quad not drawn",
                             atlas.name().data(), (unsigned)r, frame, x, y);
            }
        }
    }

    {
        const auto img = draw(s, atlas, r, frame, {1, 1, 1, 1});
        const auto px = img.pixels<Color4ub>();
        uint32_t opaque = 0;
        for (auto y = iy0; y < iy1; y++)
            for (auto x = ix0; x < ix1; x++)
                opaque += px[(uint32_t)(fb_size.y() - 1 - y)][(uint32_t)x].a() != 0;
        if (opaque == 0)
            fm_abort("%s rotation %u frame %u: sprite drew no opaque pixels",
                     atlas.name().data(), (unsigned)r, frame);
    }
}

void test_atlas(state& s, const anim_atlas& atlas)
{
    for (auto i = 0u; i < (uint32_t)rotation_COUNT; i++)
    {
        const auto r = rotation(i);
        if (!atlas.check_rotation(r))
            continue;
        const auto n = (uint32_t)atlas.group(r).frames.size();
        for (auto frame = 0u; frame < n; frame++)
            test_quad(s, atlas, r, frame);
    }
}

void test_order(state& s)
{
    // The invalid atlas is solid magenta, so the tint alone sets each sprite's color.
    const auto& atlas = *loader.invalid_anim_atlas().atlas;
    const auto& g = atlas.group(rotation::N);
    const auto& f = atlas.frame(rotation::N, 0);

    struct placement { Vector3 center; Vector4 tint; Color4ub color; };
    // Screen steps of (12, 8) on 32×32 sprites, so each overlaps the ones before it and every
    // edge falls on a whole pixel.
    constexpr placement list[] = {
        { {13, -7, 0}, {1, 0, 0, 1}, {255, 0,   0, 255} },
        { {27, -5, 0}, {0, 0, 1, 1}, {  0, 0, 255, 255} },
        { {41, -3, 0}, {0, 0, 0, 1}, {  0, 0,   0, 255} },
    };
    constexpr auto n = (uint32_t)array_size(list);
    fm_assert(f.size == Vector2ui{32});

    Vector2i p0[n];
    for (auto i = 0u; i < n; i++)
    {
        const auto p = Vector2(fb_size)*.5f + tile_shader::project(list[i].center + Vector3(g.offset)) - Vector2(f.ground);
        p0[i] = Vector2i(p);
        fm_assert(Vector2(p0[i]) == p);
    }

    // Drawn back to back with no readback between, so each draw has to keep the vertexes it was
    // issued with after the next setSubData.
    s.fb.clearColor(0, clear_color);
    s.fb.bind();
    for (const auto& x : list)
    {
        s.shader.set_tint(x.tint);
        s.sb.emit_quick(s.shader, atlas, rotation::N, 0, x.center, {1, 1, 1, 1});
    }
    const auto img = s.fb.read({{}, fb_size}, Image2D{PixelFormat::RGBA8Unorm});
    const auto px = img.pixels<Color4ub>();

    const auto size = Vector2i(f.size);
    for (auto row = 0; row < fb_size.y(); row++)
    {
        const auto y = fb_size.y() - 1 - row;
        for (auto x = 0; x < fb_size.x(); x++)
        {
            auto expected = clear_color_ub;
            for (auto i = 0u; i < n; i++)
                if (x >= p0[i].x() && x < p0[i].x() + size.x() && y >= p0[i].y() && y < p0[i].y() + size.y())
                    expected = list[i].color;
            const auto c = px[(uint32_t)row][(uint32_t)x];
            if (c != expected)
                fm_abort("pixel (%d,%d) is %02x%02x%02x%02x, should be %02x%02x%02x%02x", x, y,
                         c.r(), c.g(), c.b(), c.a(), expected.r(), expected.g(), expected.b(), expected.a());
        }
    }
}

} // namespace

void test_emit_quick()
{
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::disable(GL::Renderer::Feature::Blending);
    GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);

    state s;
    s.shader.set_scale(Vector2(fb_size));
    s.color.setStorage(GL::RenderbufferFormat::RGBA8, fb_size);
    s.fb.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, s.color);
    fm_assert(s.fb.checkStatus(GL::FramebufferTarget::Draw) == GL::Framebuffer::Status::Complete);

    test_atlas(s, *loader.scenery("table1").atlas);
    for (const auto& v : loader.vobj_list())
        test_quad(s, *v.atlas, rotation::N, 0);
    test_order(s);
}

} // namespace floormat::Test
