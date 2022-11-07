precision highp float;

uniform sampler2D sampler;
layout (location = 2) uniform vec4 tint = vec4(1, 1, 1, 1);

noperspective in vec2 frag_texcoords;
out vec4 color;

void main() {
    color = texture(sampler, frag_texcoords) * tint;
}
