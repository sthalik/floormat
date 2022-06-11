precision highp float;

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textureCoordinates;
layout(location = 0) uniform vec2 scale;

out vec2 interpolatedTextureCoordinates;
out float interpolated_frag_depth;

void main() {
    interpolatedTextureCoordinates = textureCoordinates;

    float cx = 2/scale.x, cy = 2/scale.y;
    float x = position.x, y = position.y, z = position.z;
    gl_Position = vec4((x-y)*cx, (x+y+z*2)*cx*0.75, 0, 1);
    interpolated_frag_depth = -position.z;
}
