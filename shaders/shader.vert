precision highp float;

layout (location = 0) uniform vec2 scale;
layout (location = 1) uniform vec3 offset;
layout (location = 2) uniform vec4 tint;
layout (location = 3) uniform bool enable_lightmap;

layout (location = 0) in vec4 position; // todo move depth to .w
layout (location = 1) in vec2 texcoords;
layout (location = 2) in vec2 light_coord;
layout (location = 3) in float depth;

layout (location = 0) noperspective out vec2 frag_texcoords;
layout (location = 1) noperspective out vec2 frag_light_coord;

void main() {
    float x = -position.y, y = -position.x, z = position.z;
    gl_Position = vec4((x-y+offset.x)*scale.x, ((x+y+z*2)*0.578125-offset.y)*scale.y, offset.z + depth, 1);
    frag_texcoords = texcoords;
    frag_light_coord = light_coord;
}
