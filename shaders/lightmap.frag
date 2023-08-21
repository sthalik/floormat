precision mediump float;

layout (location = 0) uniform sampler2D sampler;
layout (location = 1) uniform vec3 light_color;
layout (location = 2) uniform vec2 size;
layout (location = 3) uniform vec2 center_fragcoord;
layout (location = 4) uniform vec2 center_clip;
layout (location = 5) uniform float intensity;
layout (location = 6) uniform uint mode;
layout (location = 7) uniform uint falloff;

out vec4 color;

//layout(origin_upper_left, pixel_center_integer) in vec4 gl_FragCoord;

void main() {
    if (mode == 0)
    {
        color = vec4(0, 0, 0, 1);
    }
    else if (mode == 1) // draw
    {
        float L = intensity;
        vec2 pos = gl_FragCoord.xy;
        float dist = distance(pos, center_fragcoord);
        float A = 1;
        if (frag_falloff == 0) // linear
            A = max(0, (L - dist) / L);
        else if (frag_falloff == 2) // quadratic
        {
            float tmp = max(0, L - dist);
            A = tmp*tmp / (L*L);
        }
        color = vec4(light_color, A);
    }
    else if (mode == 2) // blend
    {
        color = texture(sampler, gl_FragCoord.xy * size);
    }
}
