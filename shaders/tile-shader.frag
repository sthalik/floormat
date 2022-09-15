precision highp float;

layout(location = 2) uniform sampler2D textureData;
layout(location = 1) uniform float y_scale;

in vec2 interpolatedTextureCoordinates;

out vec4 fragmentColor;

void main() {
    fragmentColor.rgb = texture(textureData, interpolatedTextureCoordinates).rgb;
    fragmentColor.a = 1;
}
