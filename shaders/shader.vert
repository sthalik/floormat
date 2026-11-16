precision highp float;

uniform vec2 scale;
uniform vec2 offset;
uniform uint camera_depth;

in vec4 position;
in vec3 texcoords;
in vec2 light_coord;
in uint depth;

out vec3 frag_texcoords;
out vec2 frag_light_coord;

void main() {
    const float factor = 0.5;
    float x = -position.y, y = -position.x, z = position.z;
    float d = float(int(depth - camera_depth)) * (1.0/1048576.0);
    gl_Position = vec4((x-y+offset.x)*scale.x, ((x+y+z*2.0)*factor-offset.y)*scale.y, d, 1);
    frag_texcoords = texcoords;
    frag_light_coord = light_coord;
}
