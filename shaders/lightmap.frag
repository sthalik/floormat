precision mediump float;

layout (location = 0) uniform vec4 color_intensity;
layout (location = 1) uniform vec2 center;
layout (location = 2) uniform uint falloff;
layout (location = 3) uniform vec2 size;

out vec4 color;

void main() {
    vec2 pos = gl_FragCoord.xy;
    float I = color_intensity.w;
    vec2 tmp = pos - center;
    float dist = sqrt(tmp.x*tmp.x + tmp.y*tmp.y);
    float alpha = 1 - min(1, dist / I);
    color = vec4(alpha * color_intensity.xyz, 1);
}
