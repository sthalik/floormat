#include "app.hpp"
#include "src/quads.hpp"
#include <mg/Functions.h>

namespace floormat::Test {

namespace {

using Quads::texcoords;

constexpr float eps = 1e-6f;

void assert_near(Vector2 expected, Vector2 actual, unsigned i, const char* label)
{
    if (Math::abs(expected.x() - actual.x()) > eps ||
        Math::abs(expected.y() - actual.y()) > eps)
    {
        ERR_nospace << "fatal: raw texcoord mismatch at [" << i << "] (" << label << ")";
        ERR_nospace << "  expected: " << expected;
        ERR_nospace << "    actual: " << actual;
        fm_abort("texcoord raw test failed");
    }
}

// All input values are distinct primes so every UV component is unique,
// making it trivial to trace mis-assignments in assertion failures.
constexpr Vector2ui pos{11, 23}, size{37, 51}, image_size{97, 113};

void test_normal_matches_original()
{
    auto expected = Quads::texcoords_at(pos, size, image_size);
    auto got      = Quads::texcoords_at(pos, size, image_size, false, false);
    for (unsigned i = 0; i < 4; i++)
        fm_assert_equal(expected[i], got[i]);
}

void test_mirror_only()
{
    // mirror permutation: {2, 3, 0, 1} — swap L↔R pairs
    auto base = Quads::texcoords_at(pos, size, image_size, false, false);
    auto got  = Quads::texcoords_at(pos, size, image_size, true, false);
    fm_assert_equal(base[2], got[0]);
    fm_assert_equal(base[3], got[1]);
    fm_assert_equal(base[0], got[2]);
    fm_assert_equal(base[1], got[3]);
}

void test_rotated_only()
{
    // rotation permutation: {1, 3, 0, 2}
    auto base = Quads::texcoords_at(pos, size, image_size, false, false);
    auto got  = Quads::texcoords_at(pos, size, image_size, false, true);
    fm_assert_equal(base[1], got[0]);
    fm_assert_equal(base[3], got[1]);
    fm_assert_equal(base[0], got[2]);
    fm_assert_equal(base[2], got[3]);
}

void test_mirror_and_rotated()
{
    // combined permutation: {0, 2, 1, 3}
    auto base = Quads::texcoords_at(pos, size, image_size, false, false);
    auto got  = Quads::texcoords_at(pos, size, image_size, true, true);
    fm_assert_equal(base[0], got[0]);
    fm_assert_equal(base[2], got[1]);
    fm_assert_equal(base[1], got[2]);
    fm_assert_equal(base[3], got[3]);
}

void test_all_corners_unique()
{
    auto tc = Quads::texcoords_at(pos, size, image_size, false, false);
    for (unsigned i = 0; i < 4; i++)
        for (unsigned j = i + 1; j < 4; j++)
            fm_assert_not_equal(tc[i], tc[j]);
}

void test_rotation_order_4()
{
    // the CCW rotation permutation {1,3,0,2} has order 4:
    // applying it twice gives {3,2,1,0} (reverse), not identity.
    // applying it four times gives identity.
    constexpr uint8_t perm[4] = {1, 3, 0, 2};
    uint8_t cur[4] = {0, 1, 2, 3};

    // apply once
    uint8_t tmp[4];
    for (unsigned i = 0; i < 4; i++) tmp[i] = cur[perm[i]];
    for (unsigned i = 0; i < 4; i++) cur[i] = tmp[i];
    // {1, 3, 0, 2} — not identity
    fm_assert(!(cur[0] == 0 && cur[1] == 1 && cur[2] == 2 && cur[3] == 3));

    // apply second time
    for (unsigned i = 0; i < 4; i++) tmp[i] = cur[perm[i]];
    for (unsigned i = 0; i < 4; i++) cur[i] = tmp[i];
    // {3, 2, 1, 0} — still not identity
    fm_assert(!(cur[0] == 0 && cur[1] == 1 && cur[2] == 2 && cur[3] == 3));

    // apply third time
    for (unsigned i = 0; i < 4; i++) tmp[i] = cur[perm[i]];
    for (unsigned i = 0; i < 4; i++) cur[i] = tmp[i];
    // {2, 0, 3, 1} — still not identity
    fm_assert(!(cur[0] == 0 && cur[1] == 1 && cur[2] == 2 && cur[3] == 3));

    // apply fourth time — must be identity
    for (unsigned i = 0; i < 4; i++) tmp[i] = cur[perm[i]];
    for (unsigned i = 0; i < 4; i++) cur[i] = tmp[i];
    fm_assert(cur[0] == 0 && cur[1] == 1 && cur[2] == 2 && cur[3] == 3);
}

void test_mirror_is_involution()
{
    // mirror applied twice = identity
    auto base = Quads::texcoords_at(pos, size, image_size, false, false);
    auto once = Quads::texcoords_at(pos, size, image_size, true, false);
    // manually apply mirror permutation {2,3,0,1} to 'once'
    texcoords twice = {{ once[2], once[3], once[0], once[1] }};
    for (unsigned i = 0; i < 4; i++)
        fm_assert_equal(base[i], twice[i]);
}

void test_second_set_of_values()
{
    // different distinct values to avoid only testing one input
    constexpr Vector2ui pos2{7, 19}, size2{43, 61}, image_size2{89, 127};

    auto base = Quads::texcoords_at(pos2, size2, image_size2, false, false);
    auto rot  = Quads::texcoords_at(pos2, size2, image_size2, false, true);
    fm_assert_equal(base[1], rot[0]);
    fm_assert_equal(base[3], rot[1]);
    fm_assert_equal(base[0], rot[2]);
    fm_assert_equal(base[2], rot[3]);

    auto mir  = Quads::texcoords_at(pos2, size2, image_size2, true, false);
    fm_assert_equal(base[2], mir[0]);
    fm_assert_equal(base[3], mir[1]);
    fm_assert_equal(base[0], mir[2]);
    fm_assert_equal(base[1], mir[3]);

    auto both = Quads::texcoords_at(pos2, size2, image_size2, true, true);
    fm_assert_equal(base[0], both[0]);
    fm_assert_equal(base[2], both[1]);
    fm_assert_equal(base[1], both[2]);
    fm_assert_equal(base[3], both[3]);
}

// Hand-computed in Python (double precision) via:
//   ox = px + 0.5;  oy = py + 0.5
//   ex = ox + sw - 1;  ey = oy + sh - 1
//   u0 = ox/iw;  u1 = ex/iw;  v0 = 1 - ey/ih;  v1 = 1 - oy/ih
//   corners = {BR:(u1,v0), TR:(u1,v1), BL:(u0,v0), TL:(u0,v1)}

void test_raw_set1()
{
    // pos={11,23}, size={37,51}, image_size={97,113}
    constexpr float u0 = 0.118556701030928f, u1 = 0.489690721649485f;
    constexpr float v0 = 0.349557522123894f, v1 = 0.792035398230089f;

    // --- normal: literal check ---
    constexpr texcoords expected_normal = {{
        {u1, v0}, // BR
        {u1, v1}, // TR
        {u0, v0}, // BL
        {u0, v1}, // TL
    }};
    auto got = Quads::texcoords_at({11,23}, {37,51}, {97,113}, false, false);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_normal[i], got[i], i, "set1 normal");

    // --- rotated: same literals, reordered as {1,3,0,2} ---
    constexpr texcoords expected_rot = {{
        {u1, v1}, // BR ← TR
        {u0, v1}, // TR ← TL
        {u1, v0}, // BL ← BR
        {u0, v0}, // TL ← BL
    }};
    auto got_rot = Quads::texcoords_at({11,23}, {37,51}, {97,113}, false, true);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_rot[i], got_rot[i], i, "set1 rotated");

    // --- mirrored: reordered as {2,3,0,1} ---
    constexpr texcoords expected_mir = {{
        {u0, v0}, // BR ← BL
        {u0, v1}, // TR ← TL
        {u1, v0}, // BL ← BR
        {u1, v1}, // TL ← TR
    }};
    auto got_mir = Quads::texcoords_at({11,23}, {37,51}, {97,113}, true, false);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_mir[i], got_mir[i], i, "set1 mirrored");

    // --- both: reordered as {0,2,1,3} ---
    constexpr texcoords expected_both = {{
        {u1, v0}, // BR ← BR
        {u0, v0}, // TR ← BL
        {u1, v1}, // BL ← TR
        {u0, v1}, // TL ← TL
    }};
    auto got_both = Quads::texcoords_at({11,23}, {37,51}, {97,113}, true, true);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_both[i], got_both[i], i, "set1 both");
}

void test_raw_set2()
{
    // pos={7,19}, size={43,61}, image_size={89,127}
    constexpr float u0 = 0.084269662921348f, u1 = 0.556179775280899f;
    constexpr float v0 = 0.374015748031496f, v1 = 0.846456692913386f;

    constexpr texcoords expected_normal = {{
        {u1, v0}, {u1, v1}, {u0, v0}, {u0, v1},
    }};
    auto got = Quads::texcoords_at({7,19}, {43,61}, {89,127}, false, false);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_normal[i], got[i], i, "set2 normal");

    constexpr texcoords expected_rot = {{
        {u1, v1}, {u0, v1}, {u1, v0}, {u0, v0},
    }};
    auto got_rot = Quads::texcoords_at({7,19}, {43,61}, {89,127}, false, true);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_rot[i], got_rot[i], i, "set2 rotated");
}

void test_raw_scale20()
{
    // pos={220,460}, size={740,1020}, image_size={1940,2260}
    // Same ratios as set1 × 20, but half-pixel offset and -1 don't scale,
    // so these are NOT the same UVs as set1.
    constexpr float u0 = 0.113659793814433f, u1 = 0.494587628865979f;
    constexpr float v0 = 0.345353982300885f, v1 = 0.796238938053097f;

    constexpr texcoords expected_normal = {{
        {u1, v0}, {u1, v1}, {u0, v0}, {u0, v1},
    }};
    auto got = Quads::texcoords_at({220,460}, {740,1020}, {1940,2260}, false, false);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_normal[i], got[i], i, "scale20 normal");

    // verify that scale20 UVs differ from set1 UVs
    auto set1 = Quads::texcoords_at({11,23}, {37,51}, {97,113}, false, false);
    bool any_differ = false;
    for (unsigned i = 0; i < 4; i++)
        if (Math::abs(set1[i].x() - got[i].x()) > eps ||
            Math::abs(set1[i].y() - got[i].y()) > eps)
            any_differ = true;
    fm_assert(any_differ);

    constexpr texcoords expected_rot = {{
        {u1, v1}, {u0, v1}, {u1, v0}, {u0, v0},
    }};
    auto got_rot = Quads::texcoords_at({220,460}, {740,1020}, {1940,2260}, false, true);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_rot[i], got_rot[i], i, "scale20 rotated");

    constexpr texcoords expected_both = {{
        {u1, v0}, {u0, v0}, {u1, v1}, {u0, v1},
    }};
    auto got_both = Quads::texcoords_at({220,460}, {740,1020}, {1940,2260}, true, true);
    for (unsigned i = 0; i < 4; i++)
        assert_near(expected_both[i], got_both[i], i, "scale20 both");
}

} // namespace

void test_texcoords()
{
    test_all_corners_unique();
    test_normal_matches_original();
    test_mirror_only();
    test_rotated_only();
    test_mirror_and_rotated();
    test_rotation_order_4();
    test_mirror_is_involution();
    test_second_set_of_values();
    test_raw_set1();
    test_raw_set2();
    test_raw_scale20();
}

} // namespace floormat::Test
