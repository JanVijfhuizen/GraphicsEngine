#version 330

uniform mat4 matVP;
uniform mat4 matGeo;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

out vec4 color;
out vec2 coords;
out vec2 uvs;

void main() {
   color = vec4(abs(normal), 1.0);
   vec4 pos = matVP * matGeo * vec4(pos, 1);
   gl_Position = pos;
   coords = pos.xy;
   uvs = uv;
}
