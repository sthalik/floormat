precision mediump float;

layout (location = 2) uniform sampler2D sampler0;
layout (location = 3) uniform uint mode;

layout (std140)
uniform Lightmap {
    vec4 light_color;
    vec2 scale;
    vec2 center_fragcoord;
    vec2 center_clip;
    float range;
    uint falloff;
};

layout (location = 0) out vec4 color0;
layout (location = 1) out vec4 color1;

//layout(origin_upper_left, pixel_center_integer) in vec4 gl_FragCoord;

void main() {
    if (mode == 1)
    {
        float L = range;
        vec2 pos = gl_FragCoord.xy;
        float dist = distance(pos, center_fragcoord);
        float A = 1;
        if (falloff == 0) // linear
            A = max(0, (L - dist) / L);
        else if (falloff == 2) // quadratic
        {
            float tmp = max(0, L - dist);
            A = tmp*tmp / (L*L);
        }
        color0 = vec4(light_color.rgb * light_color.a * A, 1);
    }
    else if (mode == 2) // blend
    {
        color1 = texture(sampler0, gl_FragCoord.xy * scale);
    }
    else if (mode == 0) // shadows
    {
        color0 = vec4(0, 0, 0, 1);
    }
}
