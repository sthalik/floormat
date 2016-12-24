#pragma once

class color final
{
    using uc = unsigned char;

    color(uc r, uc g, uc b, uc a = uc(255u)) : r(r), g(g), b(b), a(a) {}

public:
    const uc r, g, b, a;

    static color from_rgb(uc R, uc G, uc B, uc A = uc(255u)) { return color(R, G, B, A); }
    static color from_hsl(float H, float S, float L, float A = 1);
};

static inline color hsl(float H, float S, float L, float A) { return color::from_hsl(H, S, L, A); }
