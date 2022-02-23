uniform vec3 color = vec3(1.0, 1.0, 1.0);
layout(location = 2) uniform sampler2D textureData;
layout(location = 1) uniform float y_scale;

in vec2 interpolatedTextureCoordinates;
in float interpolated_frag_depth;

out vec4 fragmentColor;

void main() {
    fragmentColor.rgb = color*texture(textureData, interpolatedTextureCoordinates).rgb;
    fragmentColor.a = 1.0;
    gl_FragDepth = -interpolated_frag_depth * y_scale;
}
