precision highp float;
#ifdef GL_ES
precision highp sampler2DArray;
#endif

uniform vec4 tint;
uniform bool enable_lightmap;
uniform sampler2DArray sampler;
uniform sampler2D lightmap_sampler;

in vec3 frag_texcoords;
in vec2 frag_light_coord;
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
