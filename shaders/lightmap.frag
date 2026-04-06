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
    float radius;
    float _pad0, _pad1, _pad2;
};

in vec4 v_penumbras;
in vec3 v_edges;
in vec3 v_proj_pos;
in vec4 v_endpoints;

layout (location = 0) out vec4 color0;
layout (location = 1) out vec4 color1;

void main() {
    if (mode == 0u) // shadow mode: output shadow contribution
    {
        // penumbra evaluation
        mediump vec2 penumbras = smoothstep(-1.0, 1.0, v_penumbras.xz / v_penumbras.yw);
        mediump float penumbra = dot(penumbras, step(v_penumbras.yw, vec2(0.0)));
        penumbra -= 1.0/64.0; // precision fudge factor

        // edge clipping: mask out backward-projected shadows
        float edge_clip = step(v_edges.z, 0.0);

        // light penetration: soften near edge
        mediump float intersection_t = clamp(v_edges.x / abs(v_edges.y), -0.5, 0.5);
        mediump vec2 intersection_point = (0.5 - intersection_t) * v_endpoints.xy
                                        + (0.5 + intersection_t) * v_endpoints.zw;
        mediump vec2 penetration_delta = intersection_point - v_proj_pos.xy / v_proj_pos.z;
        mediump float bleed = min(dot(penetration_delta, penetration_delta), 1.0);

        float shadow = bleed * (1.0 - penumbra) * edge_clip;
        color0 = vec4(vec3(shadow), 1.0);
    }
    else if (mode == 2u) // light+blend mode: compute light * (1 - shadow_mask)
    {
        float shadow_mask = texture(sampler0, gl_FragCoord.xy * scale).r;
        float mask = clamp(1.0 - shadow_mask, 0.0, 1.0);

        float L = range;
        float dist = distance(gl_FragCoord.xy, center_fragcoord);
        float A = 1.0;
        if (falloff == 0u) // linear
            A = max(0.0, (L - dist) / L);
        else if (falloff == 2u) // quadratic
        {
            float tmp = max(0.0, L - dist);
            A = tmp * tmp / (L * L);
        }

        color1 = vec4(light_color.rgb * light_color.a * A * mask, 1.0);
    }
}
