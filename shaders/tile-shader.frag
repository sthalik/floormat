#version 450
precision highp float;

const int MAX_SAMPLERS = 16;
layout (location = 0) uniform sampler2D samplers[MAX_SAMPLERS];

layout (location = 0) in vec2 texcoord;
layout (location = 1) flat in uint sampler_id;
out vec4 fragmentColor;

void main() {
    fragmentColor.rgb = texture(samplers[sampler_id], texcoord).rgb;
    fragmentColor.a = 1;
}
