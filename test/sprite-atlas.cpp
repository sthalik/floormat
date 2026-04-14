#include "app.hpp"
#include "loader/sprite-atlas.hpp"
#include "loader/sprite-atlas-impl.hpp"
#include <algorithm>
#include <mg/Functions.h>
#include <mg/ImageView.h>
#include <mg/Range.h>
#include <Magnum/Image.h>
#include <Magnum/PixelFormat.h>

namespace floormat::SpriteAtlas {
namespace {

void two_shelves_in_same_class_do_not_overlap()
{
    Atlas a;
    a.layer_size = 256;
    auto s1 = alloc_new_shelf(a, 32).p;
    auto s2 = alloc_new_shelf(a, 32).p;
    fm_assert(s1 != s2);
    fm_assert(s1->y != s2->y);
}

void exhausting_a_layer_creates_another_layer()
{
    Atlas a;
    a.layer_size = 256;
    // Layer is 256 tall; shelves of height 64 take 64 pixels each.
    // 4 shelves fit in one layer (256 / 64 = 4); the 5th must land in a new one.
    for (int i = 0; i < 5; i++)
        alloc_new_shelf(a, 64);
    fm_assert(a.layers.size() >= 2);
}

void sprites_of_differing_sizes_create_distinct_height_classes()
{
    Atlas a;
    a.layer_size = 256;
    alloc_sprite(a, 10, 10);  // quantized height 16
    alloc_sprite(a, 10, 20);  // quantized height 24
    alloc_sprite(a, 10, 40);  // quantized height 40
    fm_assert(a.height_classes.size() == 3);
}

void alloc_more_layers_count_at_16bit_boundary()
{
    Atlas a;
    a.layer_size = 256;
    // cur=43689 is the largest uint16_t input to the 1.5x branch whose result
    // still fits in uint16_t: ceil(43689 * 1.5) + 1 == 65535.
    // At cur=43690+ the function hits its fm_assert guarding against uint16_t
    // overflow of the returned value — we can't catch that from a test, but
    // the boundary value above breaks if anyone alters the formula.
    fm_assert(alloc_more_layers_count(43689, a) == 65535);
}

// Fill a buffer with a deterministic-but-varied RGBA8 pattern so round-trip
// tests can compare byte-for-byte without random noise. The caller sizes the
// buffer as w*h*4.
void fill_pattern(unsigned char* dst, uint32_t w, uint32_t h)
{
    for (uint32_t i = 0; i < w * h; i++)
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
    a.layer_size = 256;
    constexpr uint32_t w = 4, h = 4;
    unsigned char src[w * h * 4];
    fill_pattern(src, w, h);
    Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src};

    Sprite* s = alloc_sprite(a, w, h);
    upload_sprite(a, *s, view);

    auto img = a.texture.subImage(0,
        Range3Di{{(Int)s->x, (Int)s->y, (Int)s->layer},
                 {(Int)(s->x + w), (Int)(s->y + h), (Int)(s->layer + 1)}},
        Magnum::Image3D{Magnum::PixelFormat::RGBA8Unorm});

    auto data = img.data();
    fm_assert(data.size() >= sizeof(src));
    for (size_t i = 0; i < sizeof(src); i++)
        fm_assert(data[i] == (char)src[i]);
}

void realloc_preserves_uploaded_pixels()
{
    Atlas a;
    a.layer_size = 256;
    constexpr uint32_t w = 4, h = 4;
    unsigned char src[w * h * 4];
    fill_pattern(src, w, h);
    Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, src};

    Sprite* s = alloc_sprite(a, w, h);
    upload_sprite(a, *s, view);
    auto before = a.n_layers;

    realloc_atlas(a, Math::max<uint16_t>(1, a.n_layers) * 4); // force a grow-blit
    fm_assert(a.n_layers > before);

    // Read back from the same (unchanged) coordinates after grow.
    auto img = a.texture.subImage(0,
        Range3Di{{(Int)s->x, (Int)s->y, (Int)s->layer},
                 {(Int)(s->x + w), (Int)(s->y + h), (Int)(s->layer + 1)}},
        Magnum::Image3D{Magnum::PixelFormat::RGBA8Unorm});

    auto data = img.data();
    for (size_t i = 0; i < sizeof(src); i++)
        fm_assert(data[i] == (char)src[i]);
}

void texcoords_encode_expected_corners()
{
    Atlas a;
    a.layer_size = 256;
    constexpr uint32_t w = 30, h = 40;
    Sprite* s = alloc_sprite(a, w, h);

    // texcoords_for_sprite uses direct positional UV (no 1-y flip) because
    // sub-rect atlas uploads place PNG row 0 at GL texel y=sprite.y+h-1 (top
    // of the sub-region in GL bottom-up). Expected UVs:
    //   u in [(x+0.5)/ls, (x+w-0.5)/ls]
    //   v in [(y+0.5)/ls, (y+h-0.5)/ls]
    const auto ls = (float)a.layer_size;
    const float ul = ((float)s->x + 0.5f) / ls;
    const float ur = ((float)s->x + (float)w - 0.5f) / ls;
    const float vl = ((float)s->y + 0.5f) / ls;
    const float vh = ((float)s->y + (float)h - 0.5f) / ls;
    const float layer = (float)s->layer;

    const auto coords = texcoords_for_sprite(a, *s, /*mirror=*/ false);

    // Order-agnostic: every vertex must carry the right layer, and the four
    // (u, v) pairs must span the expected bounding box.
    float umin = 999, umax = -999, vmin = 999, vmax = -999;
    for (const auto& c : coords)
    {
        fm_assert(c.z() == layer);
        umin = std::min(umin, c.x()); umax = std::max(umax, c.x());
        vmin = std::min(vmin, c.y()); vmax = std::max(vmax, c.y());
    }
    // tolerance, not ==: -Ofast's fast-math can reorder FP ops across TUs.
    constexpr float eps = 1e-6f;
    fm_assert(Math::abs(umin - ul) <= eps && Math::abs(umax - ur) <= eps);
    fm_assert(Math::abs(vmin - vl) <= eps && Math::abs(vmax - vh) <= eps);
}

void wrapper_accessors_roundtrip_values()
{
    sprite_atlas atlas{256};
    constexpr uint32_t w = 31, h = 47;
    unsigned char buf[w * h * 4]{};
    Magnum::ImageView2D view{Magnum::PixelFormat::RGBA8Unorm, {(Int)w, (Int)h}, buf};

    auto s = atlas.add(view);
    fm_assert(s.width() == w);
    fm_assert(s.height() == h);
    fm_assert(s.x() < atlas.layer_size());
    fm_assert(s.y() < atlas.layer_size());
    fm_assert(s.layer() < atlas.n_layers());
    fm_assert(!s.is_rotated());
}

void free_atlas_clears_state_and_allows_reuse()
{
    Atlas a;
    a.layer_size = 256;
    alloc_sprite(a, 10, 10);
    alloc_sprite(a, 20, 30);
    alloc_sprite(a, 40, 60);
    fm_assert(a.n_layers > 0);
    fm_assert(a.layers.size() > 0);
    fm_assert(a.height_classes.size() > 0);

    free_atlas(a);
    fm_assert(a.n_layers == 0);
    fm_assert(a.layers.size() == 0);
    fm_assert(a.height_classes.size() == 0);
    fm_assert(a.layer_size == 256); // layer_size preserved

    // Fresh batch must work.
    Sprite* s = alloc_sprite(a, 8, 8);
    fm_assert(s != nullptr);
    fm_assert(a.n_layers > 0);
}

void sprites_do_not_overlap_pairwise()
{
    Atlas a;
    a.layer_size = 256;
    constexpr uint32_t N = 40;
    struct R { uint32_t x, y, layer, w, h; };
    R rects[N];
    for (uint32_t i = 0; i < N; i++)
    {
        const uint32_t w = 5 + (i * 7) % 80;
        const uint32_t h = 3 + (i * 11) % 60;
        const Sprite* s = alloc_sprite(a, w, h);
        rects[i] = R{ (uint32_t)s->x, (uint32_t)s->y, (uint32_t)s->layer, w, h };
    }

    for (uint32_t i = 0; i < N; i++)
        for (uint32_t j = i + 1; j < N; j++)
        {
            const auto& A = rects[i];
            const auto& B = rects[j];
            if (A.layer != B.layer)
                continue;   // different layers — can't overlap
            // Standard AABB intersection test.
            const bool overlap = A.x < B.x + B.w && B.x < A.x + A.w
                              && A.y < B.y + B.h && B.y < A.y + A.h;
            fm_assert(!overlap);
        }
}

void max_dimension_sprite_stores_without_truncation()
{
    Atlas a;
    a.layer_size = max_texture_xy;   // 1024 — matches the 10-bit storage cap
    Sprite* s = alloc_sprite(a, max_texture_xy, max_texture_xy);
    fm_assert((uint32_t)s->width + 1 == max_texture_xy);
    fm_assert((uint32_t)s->height + 1 == max_texture_xy);
}

} // namespace
} // namespace floormat::SpriteAtlas

namespace floormat::Test {

void test_sprite_atlas()
{
    using namespace SpriteAtlas;
    two_shelves_in_same_class_do_not_overlap();
    exhausting_a_layer_creates_another_layer();
    sprites_of_differing_sizes_create_distinct_height_classes();
    alloc_more_layers_count_at_16bit_boundary();
    upload_and_readback_is_lossless();
    realloc_preserves_uploaded_pixels();
    texcoords_encode_expected_corners();
    wrapper_accessors_roundtrip_values();
    free_atlas_clears_state_and_allows_reuse();
    sprites_do_not_overlap_pairwise();
    max_dimension_sprite_stores_without_truncation();
}

} // namespace floormat::Test
