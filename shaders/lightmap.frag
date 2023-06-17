precision mediump float;

layout (location = 0) uniform vec4 color_intensity;
layout (location = 1) uniform vec2 center;
layout (location = 2) uniform uint falloff;
layout (location = 3) uniform vec2 size;
layout (location = 4) uniform uint mode;
layout (location = 5) uniform sampler2D sampler;

out vec4 color;

void main() {
    if (mode == 1) // draw
    {
        vec2 pos = gl_FragCoord.xy;
        float L = color_intensity.w;
        float dist = distance(pos, center);
        //float dist = sqrt(tmp.x*tmp.x + tmp.y*tmp.y);
        float A = 1;
        //attenuation = ((light_dist - dist)**exponent) / (light_dist**exponent)
        if (falloff == 0) // linear
            A = max(0, (L - dist) / L);
        else if (falloff == 2) // quadratic
        {
            float tmp = max(0, L - dist);
            A = max(0, tmp*tmp / (L*L));
        }
        //I = sqrt(color_intensity.w*1.5)*16;
        //dist = sqrt(tmp.x*tmp.x + tmp.y*tmp.y);
        //float alpha = 1 - min(1, dist / I);
        color = vec4(color_intensity.xyz, A);
    }
    else if (mode == 2) // blend
    {
        color = texture(sampler, gl_FragCoord.xy );
    }
}
