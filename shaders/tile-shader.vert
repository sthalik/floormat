precision highp float;

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textureCoordinates;
layout(location = 0) uniform vec2 projection;
layout(location = 1) uniform float y_scale;

out vec2 interpolatedTextureCoordinates;
out float interpolated_frag_depth;

void main() {
    interpolatedTextureCoordinates = textureCoordinates;

    float cx = 2/projection.x, cy = 2/projection.y;
    float x = position.x, y = position.y, z = position.z*y_scale;
    gl_Position = vec4((x-y)*cx, (x+y+z*2)*cx, 0, 1);
    interpolated_frag_depth = -position.z;
}
