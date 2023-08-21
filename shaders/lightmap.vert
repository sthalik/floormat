precision mediump float;

layout (location = 0) uniform sampler2D sampler;
layout (location = 1) uniform vec3 light_color;
layout (location = 2) uniform vec2 size;
layout (location = 3) uniform vec2 center_fragcoord;
layout (location = 4) uniform vec2 center_clip;
layout (location = 5) uniform float intensity;
layout (location = 6) uniform uint mode;
layout (location = 7) uniform uint falloff;

layout (location = 0) flat out vec3  frag_color;
layout (location = 1) flat out float frag_intensity;

layout (location = 0) in vec2 position;

void main() {
    vec2 pos = position.xy;
    vec2 dir = pos - center_clip;
    float len = length(dir);
    if (len > 1e-6)
    {
        vec2 dir_norm = dir * (1/len);
        pos += dir_norm * position.z * 4;
    }
    gl_Position = vec4(pos, 0, 1);
    frag_intensity = color_intensity.a;
    frag_falloff = uint(position_falloff.w);
}
