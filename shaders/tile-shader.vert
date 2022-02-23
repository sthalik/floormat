precision highp float;

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textureCoordinates;
layout(location = 0) uniform mat4 projection;
layout(location = 1) uniform float y_scale;

out vec2 interpolatedTextureCoordinates;
out float interpolated_frag_depth;

void main() {
    interpolatedTextureCoordinates = textureCoordinates;

    gl_Position = projection * vec4(position.xy, 0, position.w) + vec4(0, position.z * y_scale, 0, 0);
    interpolated_frag_depth = -position.z;
}
