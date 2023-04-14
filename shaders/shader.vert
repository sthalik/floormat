precision highp float;

layout (location = 0) uniform vec2 scale;
layout (location = 1) uniform vec3 offset;

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 texcoords;
layout (location = 2) in float depth;
noperspective out vec2 frag_texcoords;

void main() {
    float x = -position.y, y = -position.x, z = position.z;
    gl_Position = vec4((x-y+offset.x)*scale.x, ((x+y+z*2)*.59-offset.y)*scale.y, offset.z + depth, 1);
    frag_texcoords = texcoords;
}
