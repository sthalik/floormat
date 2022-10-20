precision highp float;

layout (location = 0) uniform vec2 scale;
layout (location = 1) uniform vec2 offset;

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 texcoords;
layout (location = 2) in float foo = 1;
noperspective out vec2 frag_texcoords;

void main() {
    float cx = 1/scale.x, cy = 1/scale.y;
    float x = -position.y, y = -position.x, z = position.z;
    gl_Position = vec4((x-y+offset.x)*cx, (x+y+z*2)*cy*.59-offset.y*cy*foo, 0, 1);
    frag_texcoords = texcoords;
}
