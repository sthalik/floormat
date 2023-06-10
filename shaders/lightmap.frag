precision mediump float;

layout (location = 0) uniform vec4 color_intensity;
layout (location = 1) uniform vec2 center;
layout (location = 2) uniform uint falloff;
layout (location = 3) uniform vec2 size;

out vec4 color;

void main() {
    vec3 color = color_intensity.xyz;
    float dist = color_intensity.w;
}
