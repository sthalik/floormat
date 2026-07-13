precision highp float;

uniform vec2 scale;
uniform vec2 offset;
uniform vec4 tint;
uniform bool enable_lightmap;
uniform sampler2DArray sampler;
uniform sampler2D lightmap_sampler;

noperspective in vec3 frag_texcoords;
noperspective in vec2 frag_light_coord;
out vec4 color;
//layout (depth_greater) out float gl_FragDepth;

void main() {
    vec4 light = tint;
    if (enable_lightmap)
        light *= vec4(texture(lightmap_sampler, frag_light_coord).rgb, 1);
    color = texture(sampler, frag_texcoords) * light;
    //if (color.a == 0)
    //    discard;
}
