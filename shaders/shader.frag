precision highp float;

layout (location = 0) uniform vec2 scale;
layout (location = 1) uniform vec3 offset;
layout (location = 2) uniform vec4 tint;
layout (location = 3) uniform bool enable_lightmap;
layout (location = 4) uniform sampler2D sampler;
layout (location = 5) uniform sampler2D lightmap_sampler;

layout (location = 0) noperspective in vec2 frag_texcoords;
layout (location = 1) noperspective in vec2 frag_light_coord;
out vec4 color;
//layout (depth_greater) out float gl_FragDepth;

void main() {
    vec4 light = tint;
    if (enable_lightmap)
        light *= vec4(texture(lightmap_sampler, frag_light_coord).rgb, 1);
    color = texture(sampler, frag_texcoords) * light;
    if (color.a == 0)
        discard;
}
