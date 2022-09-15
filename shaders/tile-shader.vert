precision highp float;

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textureCoordinates;
layout(location = 0) uniform vec2 scale;
layout(location = 3) uniform vec2 offset;

out vec2 interpolatedTextureCoordinates;

void main() {
    interpolatedTextureCoordinates = textureCoordinates;

    float cx = 2/scale.x, cy = 2/scale.y;
    float x = position.y, y = position.x, z = position.z;
    gl_Position = vec4((x-y+offset.x)*cx, (x+y+z*2)*cx*0.75-offset.y*cx, 0, 1);
}
