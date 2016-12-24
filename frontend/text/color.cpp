#include "color.hpp"

#include "util.hpp"

#include <math.h>

#ifdef __clang__
#   pragma clang diagnostic ignored "-Wcomma"
#endif

color color::from_hsl(float H, float S, float L, float A_)
{
    const float C = S * (1 - (fabsf(2 * L - 1)));
    const float H_ = H / 60;
    const float X = C * (1 - fabsf((fmodf(H_, 2) - 1)));

    float R_, G_, B_;

    if (!(H_ >= 0 && H_ < 6))
        R_ = 0, G_ = 0, B_ = 0;
    else if (H <= 1)
        R_ = C, G_ = X, B_ = 0;
    else if (H_ <= 2)
        R_ = X, G_ = C, B_ = 0;
    else if (H_ <= 3)
        R_ = 0, G_ = C, B_ = X;
    else if (H_ <= 4)
        R_ = 0, G_ = X, B_ = C;
    else if (H_ <= 5)
        R_ = X, G_ = 0, B_ = C;
    else // if (H_ < 6)
        R_ = C, G_ = 0, B_ = X;

    const float m = L - C/2;
    R_ += m, G_ += m, B_ += m;

    const uc R = uc(clamp(int(R_ * 255 + .5f), 0, 255));
    const uc G = uc(clamp(int(G_ * 255 + .5f), 0, 255));
    const uc B = uc(clamp(int(B_ * 255 + .5f), 0, 255));
    const uc A = uc(clamp(int(A_ * 255 + .5f), 0, 255));

    return color(R, G, B, A);
}
