#include "app.hpp"
#include "compat/combinations.hpp"
#include "compat/enumerate.hpp"
#include "compat/exception.inl"
#include "src/sprite-atlas.hpp"
#include "src/sprite-atlas-impl.hpp"
#include "src/sprite-constants.hpp"
#include <initializer_list>
#include <cr/GrowableArray.h>
#include <cr/StridedArrayView.h>
#include <mg/Color.h>
#include <mg/Framebuffer.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <mg/Renderbuffer.h>
#include <mg/RenderbufferFormat.h>
#include <mg/PixelFormat.h>
#include <mg/ImageView.h>
#include <mg/Image.h>
#include <mg/Texture.h>
#include <mg/TextureArray.h>

namespace floormat::SpriteAtlas {
namespace {

u32 used_height_class_count(const Atlas& a)
{
    u32 count = 0;
    for (const auto& hc : a.height_classes)
        count += !hc.shelves.isEmpty();
    return count;
}

void height_class_table_matches_quantize_height()
{
    u32 prev = 0;
    for (u32 h = 1; h <= max_sprite_xy; h++)
    {
        const u32 i = height_class_index(h);
        fm_assert(height_classes[i] == quantize_height(h));
        fm_assert(i >= prev);
        prev = i;
    }
    fm_assert(prev == height_classes.size() - 1);
}

void two_shelves_in_same_class_do_not_overlap()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    auto s1 = alloc_new_shelf(a, 32);
    auto s2 = alloc_new_shelf(a, 32);
    fm_assert(s1 != s2);
    fm_assert(s1->y != s2->y);
}

void exhausting_a_layer_creates_another_layer()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    // Layer is 256 tall; shelves of height 64 take 64 pixels each.
    // 4 shelves fit in one layer (256 / 64 = 4); the 5th must land in a new one.
    for (int i = 0; i < 5; i++)
        alloc_new_shelf(a, 64);
    fm_assert(a.layers.size() == 2);
}

void sprite_takes_other_orientation_before_new_shelf()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    alloc_sprite(a, 31, 19, false);
    const Sprite* s23 = alloc_sprite(a, 29, 23, false);
    const Sprite* s = alloc_sprite(a, 23, 31);
    fm_assert(s->is_rotated);
    fm_assert(s->layer == s23->layer && s->y == s23->y && s->x >= s23->x + s23->width);
}

void sprite_takes_orientation_wasting_less_area()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    alloc_sprite(a, 31, 19, false);
    const Sprite* s23 = alloc_sprite(a, 29, 23, false);
    const Sprite* s = alloc_sprite(a, 17, 23);
    fm_assert(!s->is_rotated);
    fm_assert(s->layer == s23->layer && s->y == s23->y);
}

void sprite_takes_taller_class_only_while_own_class_has_no_shelf()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    alloc_sprite(a, 200, 35, false);
    const Sprite* s40 = alloc_sprite(a, 10, 40, false);
    const Sprite* s35 = alloc_sprite(a, 100, 35, false);
    fm_assert(s35->y != s40->y && s35->x == 0);
    const Sprite* s31 = alloc_sprite(a, 100, 31, false);
    fm_assert(s31->layer == s35->layer && s31->y == s35->y);
}

void taller_class_takes_orientation_wasting_less_area()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    alloc_sprite(a, 10, 60, false);
    const Sprite* s36 = alloc_sprite(a, 10, 36, false);
    const Sprite* s = alloc_sprite(a, 30, 49);
    fm_assert(s->is_rotated);
    fm_assert(s->layer == s36->layer && s->y == s36->y);
}

void sprite_takes_taller_class_leaving_at_most_half_its_height_empty()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    const Sprite* s47 = alloc_sprite(a, 11, 47, false);
    const Sprite* s33 = alloc_sprite(a, 13, 33, false);
    fm_assert(s33->layer == s47->layer && s33->y == s47->y);
    const Sprite* s31 = alloc_sprite(a, 17, 31, false);
    fm_assert(s31->y != s47->y);
}

void sprites_of_differing_sizes_create_distinct_height_classes()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    alloc_sprite(a, 100, 10);  // quantized height 12
    alloc_sprite(a, 100, 20);  // quantized height 20
    alloc_sprite(a, 100, 40);  // quantized height 40
    fm_assert(used_height_class_count(a) == 3);
}

void next_layer_count_doubles_then_grows_by_half()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    constexpr u32 layers_per_gibibyte = (1u << 30) / (256 * 256 * 4);
    fm_assert(get_next_layer_count(0, a) == 1);
    u32 n = 1;
    for (; n < layers_per_gibibyte; n *= 2)
        fm_assert(get_next_layer_count(n, a) == 2 * n);
    fm_assert(n == layers_per_gibibyte);
    fm_assert(get_next_layer_count(n, a) == n + n / 2);
    fm_assert(get_next_layer_count(n + 1, a) == n + n / 2 + 2);
}

// Fill a buffer with a deterministic-but-varied RGBA8 pattern so round-trip
// tests can compare byte-for-byte without random noise. The caller sizes the
// buffer as w*h*4.
void fill_pattern(unsigned char* dst, u32 w, u32 h)
{
    for (u32 i = 0; i < w * h; i++)
    {
        dst[i*4 + 0] = (unsigned char)(i * 31);
        dst[i*4 + 1] = (unsigned char)(i * 71);
        dst[i*4 + 2] = (unsigned char)(i * 113);
        dst[i*4 + 3] = (unsigned char)(255 - i * 11);
    }
}

void upload_and_readback_is_lossless()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    constexpr u32 w = 4, h = 4;
    unsigned char src[w * h * 4];
    fill_pattern(src, w, h);
    Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src};

    Sprite* s = alloc_sprite(a, w, h);
    stage_sprite(a, *s, view);
    flush(a);

    auto img = read_layer_pixels(a.texture, (Int)s->layer, Range2Di::fromSize({(Int)s->x, (Int)s->y}, {(Int)w, (Int)h}));

    auto data = img.data();
    fm_assert(data.size() >= sizeof(src));
    for (usize i = 0; i < sizeof(src); i++)
        fm_assert(data[i] == (char)src[i]);
}

void realloc_preserves_uploaded_pixels()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    constexpr u32 w = 4, h = 4;
    unsigned char src[w * h * 4];
    fill_pattern(src, w, h);
    Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src};

    Sprite* s = alloc_sprite(a, w, h);
    stage_sprite(a, *s, view);
    flush(a);

    grow_texture(a, Math::max(u32{1}, a.texture_layer_count) * 4); // force a grow-blit

    // Read back from the same (unchanged) coordinates after grow.
    auto img = read_layer_pixels(a.texture, (Int)s->layer, Range2Di::fromSize({(Int)s->x, (Int)s->y}, {(Int)w, (Int)h}));

    auto data = img.data();
    for (usize i = 0; i < sizeof(src); i++)
        fm_assert(data[i] == (char)src[i]);
}

void grow_keeps_bound_framebuffer()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    constexpr u32 w = 4, h = 4;
    unsigned char src[w * h * 4];
    fill_pattern(src, w, h);
    Sprite* s = alloc_sprite(a, w, h);
    stage_sprite(a, *s, Magnum::ImageView2D{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src});
    flush(a);

    constexpr auto size = Vector2i{13, 11};
    constexpr auto viewport = Range2Di{{3, 2}, {12, 9}};
    GL::Renderbuffer color;
    color.setStorage(GL::RenderbufferFormat::RGBA8, size);
    GL::Framebuffer fb{viewport};
    fb.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, color);
    fm_assert(fb.checkStatus(GL::FramebufferTarget::Draw) == GL::Framebuffer::Status::Complete);
    fb.bind();
    GLint read_fb = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fb);

    grow_texture(a, Math::max(u32{1}, a.texture_layer_count) * 4);

    GLint draw_fbʹ = 0, read_fbʹ = 0, vp[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbʹ);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbʹ);
    glGetIntegerv(GL_VIEWPORT, vp);
    fm_assert_equal((GLint)fb.id(), draw_fbʹ);
    fm_assert_equal(read_fb, read_fbʹ);
    fm_assert(Range2Di::fromSize({vp[0], vp[1]}, {vp[2], vp[3]}) == viewport);

    glClearColor(0.2f, 0.4f, 0.6f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);
    const auto fb_img = fb.read({{}, size}, Magnum::Image2D{Magnum::PixelFormat::RGBA8Unorm});
    const auto fb_data = fb_img.data();
    fm_assert(fb_data.size() == usize(size.product()) * 4);
    constexpr unsigned char cleared[4] = { 51, 102, 153, 204 };
    for (const auto& [i, x] : enumerate(fb_data))
        fm_assert(x == (char)cleared[i % 4]);

    const auto img = read_layer_pixels(a.texture, (Int)s->layer,
                                       Range2Di::fromSize({(Int)s->x, (Int)s->y}, {(Int)w, (Int)h}));
    const auto data = img.data();
    for (usize i = 0; i < sizeof(src); i++)
        fm_assert(data[i] == (char)src[i]);
}

void copy_rect_keeps_pixels_and_bound_framebuffer()
{
    sprite_atlas atlas{64, 64};
    constexpr u32 w = 7, h = 11, rw = 3, rh = 13;
    unsigned char src[w * h * 4], rsrc[rw * rh * 4];
    fill_pattern(src, w, h);
    fill_pattern(rsrc, rw, rh);
    const sprite sprites[] = {
        atlas.add(Magnum::ImageView2D{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src}, false),
        atlas.add(Magnum::ImageView2D{Magnum::PixelFormat::RGBA8Unorm, {(Int)rw, (Int)rh}, rsrc}, true),
    };
    fm_assert(!sprites[0].is_rotated() && sprites[1].is_rotated());
    auto& tex = static_cast<GL::Texture2DArray&>(atlas.texture());

    constexpr auto size = Vector2i{13, 11};
    constexpr auto viewport = Range2Di{{3, 2}, {12, 9}};
    GL::Renderbuffer color;
    color.setStorage(GL::RenderbufferFormat::RGBA8, size);
    GL::Framebuffer fb{viewport};
    fb.attachRenderbuffer(GL::Framebuffer::ColorAttachment{0}, color);
    fb.bind();
    GLint read_fb = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fb);

    GL::Texture2D copies[2] = { copy_rect(*atlas.raw(), *sprites[0].raw()), copy_rect(*atlas.raw(), *sprites[1].raw()) };
    GLint draw_fbʹ = 0, read_fbʹ = 0, vp[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbʹ);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbʹ);
    glGetIntegerv(GL_VIEWPORT, vp);
    fm_assert_equal((GLint)fb.id(), draw_fbʹ);
    fm_assert_equal(read_fb, read_fbʹ);
    fm_assert(Range2Di::fromSize({vp[0], vp[1]}, {vp[2], vp[3]}) == viewport);

    for (auto i = 0u; i < 2; i++)
    {
        const auto s = sprites[i];
        const auto sw = s.is_rotated() ? s.height() : s.width(), sh = s.is_rotated() ? s.width() : s.height();
        fm_assert(copies[i].imageSize(0) == Vector2i{(Int)sw, (Int)sh});
        const auto rect_pixels = read_layer_pixels(tex, (Int)s.layer(), Range2Di::fromSize({(Int)s.x(), (Int)s.y()}, {(Int)sw, (Int)sh}));
        const auto copy = copies[i].image(0, Magnum::Image2D{Magnum::PixelFormat::RGBA8Unorm});
        fm_assert(copy.data().size() == rect_pixels.data().size());
        for (usize j = 0; j < rect_pixels.data().size(); j++)
            fm_assert(copy.data()[j] == rect_pixels.data()[j]);
    }
}

Vector2 source_texel_uv(const Atlas& a, const Sprite& s, u32 x, u32 y_from_bottom)
{
    const u32 ax = s.is_rotated ? s.x + s.height-1 - y_from_bottom : s.x + x;
    const u32 ay = s.is_rotated ? s.y + x : s.y + y_from_bottom;
    return { ((float)ax + 0.5f) / (float)a.layer_width, ((float)ay + 0.5f) / (float)a.layer_height };
}

void texcoords_show_expected_source_corners()
{
    Atlas a;
    a.layer_width = 256;
    a.layer_height = 512;
    const Sprite sprites[] = {
        { .x = 37, .y = 53, .layer = 3, .width = 29, .height = 41, .is_rotated = false },
        { .x = 61, .y = 19, .layer = 5, .width = 23, .height = 43, .is_rotated = true },
    };
    // tolerance, not ==: -Ofast's fast-math can reorder FP ops across TUs.
    constexpr float eps = 1e-6f;
    for (const Sprite& s : sprites)
    {
        const Vector2ui rects[][2] = { {{0, 0}, {s.width, s.height}}, {{5, 7}, {13, 17}}, {{3, 11}, {7, 19}} };
        for (const auto& [offset, size] : rects)
            for (bool mirror : {false, true})
            {
                const u32 left = offset.x(), right = offset.x() + size.x() - 1;
                const u32 bottom = s.height - offset.y() - size.y(), top = s.height - offset.y() - 1;
                const u32 l = mirror ? right : left, r = mirror ? left : right;
                const Vector2 expected[4] = {
                    source_texel_uv(a, s, r, bottom), source_texel_uv(a, s, r, top),
                    source_texel_uv(a, s, l, bottom), source_texel_uv(a, s, l, top),
                };
                const auto coords = texcoords_for_subrect(a, s, offset, size, mirror);
                for (auto i = 0u; i < 4; i++)
                {
                    fm_assert(coords[i].z() == (float)s.layer);
                    fm_assert(Math::abs(coords[i].x() - expected[i].x()) <= eps);
                    fm_assert(Math::abs(coords[i].y() - expected[i].y()) <= eps);
                }
            }
    }
}

Array<Color4ub> make_pixels(u32 w, u32 h, u8 seed)
{
    Array<Color4ub> ret{NoInit, w * h};
    for (u32 i = 0; i < w * h; i++)
        ret[i] = {(u8)i, (u8)(i >> 8), seed, 255};
    return ret;
}

ImageView2D view_of(const Array<Color4ub>& pixels, u32 w, u32 h)
{
    fm_assert(pixels.size() == w * h);
    return ImageView2D{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, pixels};
}

struct sprite_pixels { sprite s; ArrayView<const Color4ub> pixels; };

void assert_layer_contents(sprite_atlas& atlas, u32 layer, std::initializer_list<sprite_pixels> sprites)
{
    constexpr auto magenta = Color4ub{255, 0, 255, 255};
    auto& tex = static_cast<GL::Texture2DArray&>(atlas.texture());
    const auto w = atlas.layer_width(), h = atlas.layer_height();
    auto img = read_layer_pixels(tex, (Int)layer, {{}, {(Int)w, (Int)h}});
    const auto texels = img.pixels<Color4ub>();
    for (const auto& [s, pixels] : sprites)
        fm_assert(!s.is_rotated() && pixels.size() == s.width() * s.height());
    for (u32 y = 0; y < h; y++)
        for (u32 x = 0; x < w; x++)
        {
            auto expected = magenta;
            for (const auto& [s, pixels] : sprites)
                if (s.layer() == layer && x >= s.x() && x < s.x() + s.width() && y >= s.y() && y < s.y() + s.height())
                    expected = pixels[(y - s.y()) * s.width() + x - s.x()];
            fm_assert(texels[y][x] == expected);
        }
}

void texture_growth_clears_new_layers_to_magenta()
{
    sprite_atlas atlas{64, 64};
    constexpr u32 w = 40, h = 40;
    const auto pixels0 = make_pixels(w, h, 1), pixels1 = make_pixels(w, h, 2);
    const auto s0 = atlas.add(view_of(pixels0, w, h), false);
    assert_layer_contents(atlas, 0, {{s0, pixels0}});
    const auto s1 = atlas.add(view_of(pixels1, w, h), false);
    fm_assert(s1.layer() == 1);
    assert_layer_contents(atlas, 0, {{s0, pixels0}});
    assert_layer_contents(atlas, 1, {{s1, pixels1}});
}

void reset_discards_pending_sprites()
{
    sprite_atlas atlas{64, 64};
    const auto a = make_pixels(40, 40, 1), b = make_pixels(7, 11, 2), c = make_pixels(40, 40, 3);
    atlas.add(view_of(a, 40, 40), false);
    (void)atlas.texture();
    const auto sb = atlas.add(view_of(b, 7, 11), false);
    fm_assert(sb.layer() == 0);
    atlas.reset();
    const auto sc = atlas.add(view_of(c, 40, 40), false);
    assert_layer_contents(atlas, 0, {{sc, c}});
}

void rgb_sprites_read_back_opaque()
{
    sprite_atlas atlas{64, 64};
    PixelStorage storage;
    storage.setAlignment(1);
    constexpr u32 w = 7, h = 11;
    u8 rgb[w * h * 3];
    for (u32 i = 0; i < w * h * 3; i++)
        rgb[i] = (u8)(i * 7 + 1);
    const ImageView2D view{storage, Magnum::PixelFormat::RGB8Unorm, {(Int)w, (Int)h}, rgb};
    const sprite sprites[] = { atlas.add(view, true), atlas.add(view, false) };
    fm_assert(sprites[0].is_rotated() && !sprites[1].is_rotated());
    auto& tex = static_cast<GL::Texture2DArray&>(atlas.texture());
    for (const auto s : sprites)
    {
        const auto sw = s.is_rotated() ? h : w, sh = s.is_rotated() ? w : h;
        auto img = read_layer_pixels(tex, (Int)s.layer(), Range2Di::fromSize({(Int)s.x(), (Int)s.y()}, {(Int)sw, (Int)sh}));
        const auto texels = img.pixels<Color4ub>();
        for (u32 y = 0; y < h; y++)
            for (u32 x = 0; x < w; x++)
            {
                const u8* p = rgb + (y * w + x) * 3;
                const auto texel = s.is_rotated() ? texels[x][h-1 - y] : texels[y][x];
                fm_assert(texel == Color4ub(p[0], p[1], p[2], 255));
            }
    }
}

void add_rejects_bad_format_and_size()
{
    sprite_atlas atlas{64, 64};
    const char r8[4 * 4] = {};
    bool caught = false;
    try { atlas.add(ImageView2D{Magnum::PixelFormat::R8Unorm, {4, 4}, r8}); }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(caught);

    const auto too_wide = make_pixels(65, 4, 1);
    caught = false;
    try { atlas.add(view_of(too_wide, 65, 4)); }
    catch (const floormat::exception&) { caught = true; }
    fm_assert(caught);
    fm_assert(atlas.layer_count() == 0 && atlas.raw()->pending.isEmpty());
}

// add() only stages pixels. texture() grows the texture over every layer packed since and uploads
// them, a rotated sprite transposed.
void staged_adds_reach_texture()
{
    sprite_atlas atlas{64, 64};
    // 40x40 leaves no room for a second shelf in a 64 px layer, so each takes a new layer
    constexpr u32 n = 3, w = 40, h = 40, rw = 7, rh = 11;
    Array<unsigned char> src[n + 1];
    const Sprite* sprites[n + 1];
    for (auto i = 0u; i <= n; i++)
    {
        const auto sw = i < n ? w : rw, sh = i < n ? h : rh;
        src[i] = Array<unsigned char>{NoInit, sw*sh*4};
        fill_pattern(src[i].data(), sw, sh);
        for (auto& c : src[i])
            c = (unsigned char)(c + 37*i);
        const Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)sw, (Int)sh}, src[i]};
        sprites[i] = atlas.add(view, i == n).raw();
    }
    fm_assert(sprites[n]->is_rotated);
    fm_assert(sprites[n-1]->layer == n-1);
    fm_assert(atlas.raw()->texture.id() == 0 && atlas.texture_gen() == 0 && atlas.raw()->pending.size() == n + 1);

    auto& tex = static_cast<GL::Texture2DArray&>(atlas.texture());
    fm_assert(atlas.layer_count() == n && tex.imageSize(0).z() == 4);
    // The flush in texture() empties both but keeps what stage_sprite() reserved.
    fm_assert(atlas.raw()->staging.isEmpty() && arrayCapacity(atlas.raw()->staging) >= 1 << 20);
    fm_assert(atlas.raw()->pending.isEmpty() && arrayCapacity(atlas.raw()->pending) >= 64);
    for (auto i = 0u; i <= n; i++)
    {
        const auto s = sprite{sprites[i]};
        const auto sw = s.is_rotated() ? s.height() : s.width(), sh = s.is_rotated() ? s.width() : s.height();
        auto img = read_layer_pixels(tex, (Int)s.layer(), Range2Di::fromSize({(Int)s.x(), (Int)s.y()}, {(Int)sw, (Int)sh}));
        const auto* rect_pixels = reinterpret_cast<const u32*>(img.data().data());
        const auto* orig = reinterpret_cast<const u32*>(src[i].data());
        for (auto y = 0u; y < s.height(); y++)
            for (auto x = 0u; x < s.width(); x++)
                fm_assert(orig[y*s.width() + x] == (s.is_rotated() ? rect_pixels[x*sw + s.height()-1-y] : rect_pixels[y*sw + x]));
    }
}

void texture_growth_stops_at_layer_limit()
{
    sprite_atlas atlas{64, 64};
    atlas.raw()->limits = { .size = 64, .layers = 3, };
    constexpr u32 n = 3, w = 40, h = 40;
    Array<unsigned char> src{ValueInit, w*h*4};
    const Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src};
    for (auto i = 0u; i < n; i++)
        atlas.add(view, false);
    auto& tex = static_cast<GL::Texture2DArray&>(atlas.texture());
    fm_assert(atlas.layer_count() == n && tex.imageSize(0).z() == (Int)n);
}

void each_texture_gets_new_texture_gen()
{
    sprite_atlas atlas{64, 64};
    constexpr u32 w = 40, h = 40;
    Array<unsigned char> src{ValueInit, w*h*4};
    const Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src};
    const auto gen0 = atlas.texture_gen();
    atlas.add(view, false);
    (void)atlas.texture();
    const auto gen1 = atlas.texture_gen();
    const auto layers = atlas.raw()->texture_layer_count;
    fm_assert_equal(gen0 + 1, gen1);
    grow_texture(*atlas.raw(), layers * 2);
    fm_assert_equal(gen1 + 1, atlas.texture_gen());
    atlas.reset();
    fm_assert_equal(gen1 + 1, atlas.texture_gen());
    atlas.add(view, false);
    (void)atlas.texture();
    fm_assert_equal(layers, atlas.raw()->texture_layer_count);
    fm_assert_equal(gen1 + 2, atlas.texture_gen());
}

void wrapper_accessors_roundtrip_values()
{
    sprite_atlas atlas{256, 256};
    constexpr u32 w = 31, h = 47;
    unsigned char buf[w * h * 4]{};
    Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, buf};

    auto s = atlas.add(view, false);
    fm_assert(s.width() == w);
    fm_assert(s.height() == h);
    fm_assert(s.x() < atlas.layer_width());
    fm_assert(s.y() < atlas.layer_height());
    fm_assert(s.layer() < atlas.layer_count());
    fm_assert(!s.is_rotated());
}

void free_atlas_clears_state_and_allows_reuse()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    alloc_sprite(a, 10, 10);
    alloc_sprite(a, 20, 30);
    alloc_sprite(a, 40, 60);
    fm_assert(a.layers.size() > 0);
    fm_assert(used_height_class_count(a) > 0);

    free_atlas(a);
    fm_assert(a.texture_layer_count == 0);
    fm_assert(a.layers.size() == 0);
    fm_assert(used_height_class_count(a) == 0);
    fm_assert(a.layer_width == 256 && a.layer_height == 256);

    // Fresh batch must work.
    Sprite* s = alloc_sprite(a, 8, 8);
    fm_assert(s != nullptr);
    fm_assert(a.layers.size() > 0);
}

void sprites_do_not_overlap_pairwise()
{
    Atlas a;
    a.layer_width = a.layer_height = 256;
    constexpr u32 N = 40;
    struct R { u32 x, y, layer, w, h; };
    R rects[N];
    for (auto& [i, r] : enumerate(rects))
    {
        const u32 w = 5 + (i * 7) % 80;
        const u32 h = 3 + (i * 11) % 60;
        const Sprite* s = alloc_sprite(a, w, h, false);
        r = R{ (u32)s->x, (u32)s->y, (u32)s->layer, w, h };
    }

    for (const auto& [A, B] : combinations(rects))
    {
        if (A.layer != B.layer)
            continue;   // different layers — can't overlap
        // Standard AABB intersection test.
        const bool overlap = A.x < B.x + B.w && B.x < A.x + A.w
                          && A.y < B.y + B.h && B.y < A.y + A.h;
        fm_assert(!overlap);
    }
}

void rotated_sprites_stay_in_layer_and_do_not_overlap()
{
    Atlas a;
    a.layer_width = a.layer_height = max_sprite_xy;
    constexpr u32 N = 400;
    struct R { u32 x, y, layer, w, h; };
    R rects[N];
    u32 rotated_count = 0;
    for (auto& [i, r] : enumerate(rects))
    {
        const u32 w = 3 + (i * 37) % 290, h = 2 + (i * 53) % 230;
        const Sprite* s = alloc_sprite(a, w, h);
        rotated_count += s->is_rotated;
        r = R{ s->x, s->y, s->layer, s->is_rotated ? h : w, s->is_rotated ? w : h };
        fm_assert(r.x + r.w <= a.layer_width && r.y + r.h <= a.layer_height);
    }
    fm_assert(rotated_count > 0);

    for (const auto& [A, B] : combinations(rects))
    {
        if (A.layer != B.layer)
            continue;
        const bool overlap = A.x < B.x + B.w && B.x < A.x + A.w
                          && A.y < B.y + B.h && B.y < A.y + A.h;
        fm_assert(!overlap);
    }
}

void max_dimension_sprite_stores_without_truncation()
{
    Atlas a;
    a.layer_width = a.layer_height = max_sprite_xy;
    Sprite* s = alloc_sprite(a, max_sprite_xy, max_sprite_xy);
    fm_assert(s->width  == max_sprite_xy);
    fm_assert(s->height == max_sprite_xy);
}

void tallest_layer_stores_y_without_truncation()
{
    Atlas a;
    a.layer_width = 64;
    a.layer_height = max_layer_height;
    constexpr u32 w = 64, h = 16, N = max_layer_height / h;
    const Sprite* sprites[N];
    for (u32 i = 0; i < N; i++)
        sprites[i] = alloc_sprite(a, w, h, false);

    Array<bool> used{ValueInit, (usize)a.layers.size() * max_layer_height};
    for (const auto* s : sprites)
    {
        fm_assert(s->y + h <= max_layer_height);
        for (u32 y = (u32)s->y; y < s->y + h; y++)
        {
            auto& u = used[(u32)s->layer * max_layer_height + y];
            fm_assert(!u);
            u = true;
        }
    }
}

} // namespace
} // namespace floormat::SpriteAtlas

namespace floormat::Test {

void test_sprite_atlas()
{
    using namespace SpriteAtlas;
    height_class_table_matches_quantize_height();
    two_shelves_in_same_class_do_not_overlap();
    exhausting_a_layer_creates_another_layer();
    sprite_takes_other_orientation_before_new_shelf();
    sprite_takes_orientation_wasting_less_area();
    sprite_takes_taller_class_only_while_own_class_has_no_shelf();
    taller_class_takes_orientation_wasting_less_area();
    sprite_takes_taller_class_leaving_at_most_half_its_height_empty();
    sprites_of_differing_sizes_create_distinct_height_classes();
    next_layer_count_doubles_then_grows_by_half();
    upload_and_readback_is_lossless();
    realloc_preserves_uploaded_pixels();
    grow_keeps_bound_framebuffer();
    copy_rect_keeps_pixels_and_bound_framebuffer();
    texcoords_show_expected_source_corners();
    texture_growth_clears_new_layers_to_magenta();
    reset_discards_pending_sprites();
    rgb_sprites_read_back_opaque();
    add_rejects_bad_format_and_size();
    staged_adds_reach_texture();
    texture_growth_stops_at_layer_limit();
    each_texture_gets_new_texture_gen();
    wrapper_accessors_roundtrip_values();
    free_atlas_clears_state_and_allows_reuse();
    sprites_do_not_overlap_pairwise();
    rotated_sprites_stay_in_layer_and_do_not_overlap();
    max_dimension_sprite_stores_without_truncation();
    tallest_layer_stores_y_without_truncation();
}

} // namespace floormat::Test
