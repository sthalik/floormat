precision mediump float;

layout (location = 2) uniform sampler2D sampler0;
layout (location = 3) uniform uint mode;

layout (std140)
uniform Lightmap {
    vec4 light_color;
    vec2 scale;
    vec2 center_fragcoord;
    vec2 center_clip;
    float range;
    uint falloff;
};

layout (location = 0) in vec3 position;

void main() {
    // todo add interface blocks
    vec2 pos = position.xy;
    if (mode == 0)
    {
        vec2 dir = pos - center_clip;
        pos += dir * position.z * 1e4;
    }
    gl_Position = vec4(pos, 0, 1);
}
