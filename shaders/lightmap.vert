precision mediump float;

layout (location = 2) uniform sampler2D sampler0;
layout (location = 3) uniform vec4 light_color;
layout (location = 4) uniform vec2 scale;
layout (location = 5) uniform vec2 center_fragcoord;
layout (location = 6) uniform vec2 center_clip;
layout (location = 7) uniform float range;
layout (location = 8) uniform uint mode;
layout (location = 9) uniform uint falloff;

//layout (location = 0) out vec2 frag_texcoords;
//layout (location = 1) flat out vec2 frag_light_coord;

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
