precision mediump float;

layout (location = 0) uniform vec4 color_intensity;
layout (location = 1) uniform vec2 center;
layout (location = 2) uniform uint falloff;
layout (location = 3) uniform vec2 size;

layout (location = 0) in vec4 position;

void main() {
    gl_Position = vec4(position.x, position.y, 0, 1);
}
