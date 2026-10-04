#include "app.hpp"
#include "compat/array-size.hpp"
#include "src/spritebatch.hpp"
#include "src/anim-atlas.hpp"
#include "src/anim.hpp"
#include "src/scenery-proto.hpp"
#include "src/rotation.hpp"
#include "src/sprite-atlas.hpp"
#include "src/sprite-atlas-impl.hpp"
#include "loader/loader.hpp"
#include "loader/anim-cell.hpp"
#include "loader/scenery-cell.hpp"
#include "loader/vobj-cell.hpp"
#include "shaders/shader.hpp"
#include "shaders/texture-unit-cache.hpp"
#include <cstring>
#include <cr/StridedArrayView.h>
#include <mg/Color.h>
#include <mg/Framebuffer.h>
#include <mg/Image.h>
#include <mg/PixelFormat.h>
#include <mg/Renderbuffer.h>
#include <mg/RenderbufferFormat.h>
#include <mg/Renderer.h>
#include <mg/TextureArray.h>

namespace floormat::Test {

namespace {

constexpr Vector2i fb_size{512, 512};
// x+y differs in parity between the two, so one of them projects to a half pixel.
constexpr Vector3i anchors[] = { {13, -7, 0}, {14, -7, 0} };
constexpr uint32_t margin = 2;
// Blending is off, so the quad writes every pixel it covers, transparent texels included.
constexpr Color4 clear_color{1, 0, 1, 1};
constexpr Color4ub clear_color_ub{255, 0, 255, 255};

struct state
{
    texture_unit_cache tuc;
    tile_shader shader{tuc};
    SpriteBatch sb;
    GL::Renderbuffer color;
    GL::Framebuffer fb{{{}, fb_size}};
};

// The frame's texels inside `margin` pixels of clear color, rows bottom-up as a readback returns
// them. The atlas stores a rotated sprite transposed.
Array<Color4ub> expected_pixels(const anim_atlas& atlas, rotation r, uint32_t frame)
{
    const auto& g = atlas.group(r);
    const auto& sp = *g.sprites[frame];
    const auto w = (uint32_t)sp.width + 1, h = (uint32_t)sp.height + 1;
    fm_assert(Vector2ui{w, h} == atlas.frame(r, frame).size);
    const bool rotated = sp.is_rotated, mirror = !g.mirror_from.isEmpty();
    const auto sw = rotated ? h : w, sh = rotated ? w : h;
    const auto img = loader.atlas().raw()->texture.subImage(0,
        Range3Di{{(Int)sp.x, (Int)sp.y, (Int)sp.layer}, {(Int)(sp.x + sw), (Int)(sp.y + sh), (Int)sp.layer + 1}},
        Image3D{PixelFormat::RGBA8Unorm});
    const auto* slot = reinterpret_cast<const Color4ub*>(img.data().data());
    const auto cw = w + 2*margin;
    Array<Color4ub> ret{DirectInit, (size_t)cw*(h + 2*margin), clear_color_ub};
    for (auto y = 0u; y < h; y++)
        for (auto x = 0u; x < w; x++)
            ret[(y + margin)*cw + margin + (mirror ? w-1-x : x)] = rotated ? slot[x*sw + h-1-y] : slot[y*w + x];
    return ret;
}

void test_quad(state& s, const anim_atlas& atlas, rotation r, uint32_t frame, Vector3i anchor,
               ArrayView<const Color4ub> expected)
{
    const auto& f = atlas.frame(r, frame);
    const auto m = Vector2i{(Int)margin};
    // The clickable rect's corner, so the drawn pixels are the ones the bitmask hit-tests.
    const auto p0 = tile_shader::point_to_pixelʹ(anchor + Vector3i(atlas.group(r).offset), fb_size, {}) - Vector2i(f.ground);
    const auto lo = p0 - m, hi = p0 + Vector2i(f.size) + m;
    fm_assert(lo.x() >= 0 && lo.y() >= 0 && hi.x() <= fb_size.x() && hi.y() <= fb_size.y());

    s.fb.clearColor(0, clear_color);
    s.fb.bind();
    s.shader.set_tint({1, 1, 1, 1});
    s.sb.emit_quick(s.shader, atlas, r, frame, Vector3(anchor), {1, 1, 1, 1});
    const auto img = s.fb.read({{lo.x(), fb_size.y() - hi.y()}, {hi.x(), fb_size.y() - lo.y()}},
                               Image2D{PixelFormat::RGBA8Unorm});
    fm_assert(img.data().size() == expected.size()*sizeof(Color4ub));
    const auto* drawn = reinterpret_cast<const Color4ub*>(img.data().data());
    if (!std::memcmp(drawn, expected.data(), img.data().size()))
        return;

    const auto w = (uint32_t)(hi.x() - lo.x()), h = (uint32_t)(hi.y() - lo.y());
    uint32_t bad = 0, first = 0;
    for (auto i = 0u; i < (uint32_t)expected.size(); i++)
        if (drawn[i] != expected[i] && !bad++)
            first = i;
    // Reported y down from the sprite's corner. Readback rows go bottom-up.
    const auto fx = (Int)(first % w) - (Int)margin, fy = (Int)(h - 1 - first / w) - (Int)margin;
    const auto c = drawn[first], e = expected[first];
    fm_abort("%s rotation %u frame %u at (%d,%d,%d): %u pixels differ from the atlas, first at (%d,%d) "
             "is %02x%02x%02x%02x, should be %02x%02x%02x%02x",
             atlas.name().data(), (unsigned)r, frame, anchor.x(), anchor.y(), anchor.z(), bad, fx, fy,
             c.r(), c.g(), c.b(), c.a(), e.r(), e.g(), e.b(), e.a());
}

void test_frame(state& s, const anim_atlas& atlas, rotation r, uint32_t frame)
{
    const auto expected = expected_pixels(atlas, r, frame);
    for (const auto& anchor : anchors)
        test_quad(s, atlas, r, frame, anchor, expected);
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
            test_frame(s, atlas, r, frame);
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

    for (const auto& x : loader.scenery_list())
        if (x.name != loader.INVALID)
            test_atlas(s, *loader.scenery(x.name).atlas);
    test_atlas(s, *loader.anim_atlas("npc-walk", loader.ANIM_PATH));
    for (const auto& v : loader.vobj_list())
        test_frame(s, *v.atlas, rotation::N, 0);
    test_order(s);
}

} // namespace floormat::Test
