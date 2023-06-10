precision mediump float;

layout (location = 0) uniform vec4 color_intensity;
layout (location = 1) uniform vec2 center;
layout (location = 2) uniform uint falloff;
layout (location = 3) uniform vec2 size;

out vec4 color;

void main() {
    vec2 pos = gl_FragCoord.xy;
    float I = color_intensity.w;
    float dist = distance(pos, center);
    //float dist = sqrt(tmp.x*tmp.x + tmp.y*tmp.y);
    float A = 1;
    if (falloff == 0) // linear
        A = 1 - min(1, dist / I);
    else if (falloff == 2) // quadratic
        A = I/(1 + I + dist*dist);
    //I = sqrt(color_intensity.w*1.5)*16;
    //dist = sqrt(tmp.x*tmp.x + tmp.y*tmp.y);
    //float alpha = 1 - min(1, dist / I);
    color = vec4(color_intensity.xyz, A);
}
