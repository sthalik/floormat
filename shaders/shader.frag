precision highp float;

uniform sampler2D sampler;
layout (location = 2) uniform vec4 tint = vec4(1, 1, 1, 1);

noperspective in vec2 frag_texcoords;
out vec4 color;
//layout (depth_greater) out float gl_FragDepth;

void main() {
    color = texture(sampler, frag_texcoords) * tint;
    if (color.a == 0)
        discard;
}
