precision highp float;

layout(location = 2) uniform sampler2D textureData;

in vec2 interpolatedTextureCoordinates;

out vec4 fragmentColor;

void main() {
    fragmentColor.rgb = texture(textureData, interpolatedTextureCoordinates).rgb;
    fragmentColor.a = 1;
}
