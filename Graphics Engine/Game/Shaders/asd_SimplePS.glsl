#version 330

in vec4 color;
in vec2 coords;
in vec2 uvs;
out vec4 outColor;

uniform sampler2D diffuse;
uniform float time;

void main() {
	float ripple = mod(time, 4) * 2.0 - 1.0;
	float dis = pow(length(coords), 2);
	float off = abs(ripple - dis);
	float res = smoothstep(off, off + .1, .4) * dis;
	
	outColor = texture(diffuse, uvs + res) * color;
}