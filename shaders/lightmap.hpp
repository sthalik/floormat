#pragma once

#include "light-falloff.hpp"
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>

namespace floormat {

struct local_coords;

struct lightmap_shader final : GL::AbstractShaderProgram
{
    using Position = GL::Attribute<0, Vector2>;

    explicit lightmap_shader();
    ~lightmap_shader() override;

    void set_light(Vector2i neighbor_offset, local_coords pos, Vector2b offset);
    struct light light() const; // is a reader accessor needed?

private:
    static Vector2i get_px_pos(Vector2i neighbor_offset, local_coords pos, Vector2b offset);

    struct light_u final
    {
        Vector4 color_and_intensity;
        Vector2 center;
        uint32_t mode;
    };

    struct light_s final
    {
        float intensity = 1;
        Color3ub color {255, 255, 255};
        Vector2i center;
        light_falloff falloff;

        bool operator==(const light_s&) const noexcept;
    };

    enum { ColorUniform = 0, CenterUniform = 1, FalloffUniform = 2, DepthUniform = 3, };

    light_s _light;
};

} // namespace floormat
