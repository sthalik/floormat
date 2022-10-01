precision highp float;

layout (location = 0) uniform vec2 scale;
layout (location = 1) uniform vec2 offset;
uniform sampler2D samplers[32];

in vec4 position;
in vec2 texcoords;
in uint sampler_id;
out vec2 out_texcoords;
flat out uint frag_sampler_id;

void main() {
    out_texcoords = texcoords;
    frag_sampler_id = sampler_id;

    float cx = 2/scale.x, cy = 2/scale.y;
    float x = position.y, y = position.x, z = position.z;
    gl_Position = vec4((x-y+offset.x)*cx, (x+y+z*2)*cx*0.75-offset.y*cx, 0, 1);
}
