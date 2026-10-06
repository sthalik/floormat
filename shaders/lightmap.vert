precision highp float;

uniform uint mode;

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

// shadow geometry attributes
layout (location = 0) in vec4 a_segment;      // endpoint_a.xy, endpoint_b.xy
layout (location = 1) in vec2 a_shadow_coord;  // x: endpoint select, y: near/far

// light quad attribute (reuses location 0 as vec3)
// layout (location = 0) in vec3 position;

// Perspective-correct: the far vertices have w = 0, and the fragment shader uses only
// ratios of these, which cancel the interpolated w. noperspective comes out NaN on Mesa.
out vec4 v_penumbras;
out vec3 v_edges; // z = edge clip, xy = light penetration
out vec3 v_proj_pos;
out vec4 v_endpoints;

mat2 adjugate(mat2 m) {
    return mat2(m[1][1], -m[0][1], -m[1][0], m[0][0]);
}

void main() {
    if (mode == 0u) // shadow mode
    {
        vec2 endpoint_a = a_segment.xy;
        vec2 endpoint_b = a_segment.zw;
        vec2 endpoint = mix(endpoint_a, endpoint_b, a_shadow_coord.x);

        // deltas from endpoints to light center
        vec2 delta_a = endpoint_a - center_clip;
        vec2 delta_b = endpoint_b - center_clip;
        vec2 delta   = endpoint   - center_clip;

        // perpendicular offsets for penumbra expansion
        float lr = radius;
        // convert light radius from pixel-space to clip-space
        // scale is 1/real_image_size, clip_scale is 2/image_size
        // we need radius in clip units: radius_px * (2 / real_image_size)
        float lr_clip = lr * scale.x * 2.0;

        // cap lr_clip so the shadow quad's angular span
        // stays under 180°. Beyond that, the projected far vertices cross and
        // the quad degenerates (split winding → slivers with face culling,
        // shadow wrap-around without it). The break point is:
        //   lr_clip_max = min_dist * tan((π - α) / 2) = min_dist * (1 + cos α) / sin α
        // where α is the segment's angular span seen from the light.
        // The 0.98 factor keeps a safety margin below the degeneration boundary.
        // No acos()/tan(): acos() rounds α to 0 for a segment seen edge-on, and tan(float(π)/2) < 0.
        float len_a = length(delta_a);
        float len_b = length(delta_b);
        float min_dist = min(len_a, len_b);
        vec2 dir_a = delta_a / len_a, dir_b = delta_b / len_b;
        float cos_alpha = dot(dir_a, dir_b);
        float sin_alpha = abs(dir_a.x * dir_b.y - dir_a.y * dir_b.x);
        float max_lr = 0.98 * min_dist * (1.0 + cos_alpha);
        lr_clip = min(lr_clip, max_lr / max(sin_alpha, 1e-30));

        vec2 offset_a = vec2(-lr_clip,  lr_clip) * normalize(delta_a).yx;
        vec2 offset_b = vec2( lr_clip, -lr_clip) * normalize(delta_b).yx;
        vec2 offset   = mix(offset_a, offset_b, a_shadow_coord.x);

        // project: w=1 for near vertices, w=0 for far (projected to infinity)
        float w = a_shadow_coord.y;
        vec2 proj_pos_2d = mix(delta - offset, endpoint, w);

        // penumbra gradient coordinates via adjugate matrix
        vec2 penumbra_a = adjugate(mat2( offset_a, -delta_a)) * (delta - mix(offset, delta_a, w));
        vec2 penumbra_b = adjugate(mat2(-offset_b,  delta_b)) * (delta - mix(offset, delta_b, w));

        v_penumbras = (lr_clip > 0.0) ? vec4(penumbra_a, penumbra_b) : vec4(0, 1, 0, 1);

        // edge clipping (prevents backward shadow projection)
        vec2 seg_delta  = endpoint_b - endpoint_a;
        vec2 seg_normal = seg_delta.yx * vec2(-1.0, 1.0);
        v_edges.z = dot(seg_normal, delta - offset) * (1.0 - w);

        // light penetration
        float light_penetration = 0.01;
        v_edges.xy = -adjugate(mat2(seg_delta, delta_a + delta_b)) * (delta - offset * (1.0 - w));
        v_edges.y *= 2.0;

        v_proj_pos = vec3(proj_pos_2d, w * light_penetration);
        v_endpoints = vec4(endpoint_a, endpoint_b) / light_penetration;

        gl_Position = vec4(proj_pos_2d, 0, w);
    }
    else // light+blend mode (mode == 2): full-screen quad passthrough
    {
        v_penumbras = vec4(0);
        v_edges = vec3(0);
        v_proj_pos = vec3(0);
        v_endpoints = vec4(0);
        // a_segment.xy is actually the position.xy for the light quad
        gl_Position = vec4(a_segment.xy, 0, 1);
    }
}
