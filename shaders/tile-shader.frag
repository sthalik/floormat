precision highp float;

uniform sampler2D samplers[32];

in vec2 out_texcoords;
flat in uint frag_sampler_id;
out vec4 fragmentColor;

void main() {
    fragmentColor.rgb = texture(samplers[frag_sampler_id], out_texcoords).rgb;
    fragmentColor.a = 1;
}
