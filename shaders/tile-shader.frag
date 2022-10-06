precision highp float;

uniform sampler2DRect sampler;

noperspective in vec2 frag_texcoords;
out vec4 color;

void main() {
    color = vec4(texture(sampler, frag_texcoords).rgb, 1);
}
