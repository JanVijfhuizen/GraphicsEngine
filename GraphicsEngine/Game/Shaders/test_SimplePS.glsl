#version 330

uniform vec2 uResolution;
uniform float uTime;
uniform ivec2 uSimResolution;

out vec4 outColor;

float rand(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 p){
	vec2 ip = floor(p);
	vec2 u = fract(p);
	u = u*u*(3.0-2.0*u);
	
	float res = mix(
		mix(rand(ip),rand(ip+vec2(1.0,0.0)),u.x),
		mix(rand(ip+vec2(0.0,1.0)),rand(ip+vec2(1.0,1.0)),u.x),u.y);
	return res*res;
}

void main()
{
 	float m = uResolution.x / uResolution.y;
    vec2 res = vec2(uSimResolution.x * m, uSimResolution.y);

    vec2 uv = gl_FragCoord.xy/uResolution;
    uv *= res;
    uv = floor(uv);
    uv /= res;
    
    vec4 c = vec4(vec3(0.0), 1.0);
    
    float nx = noise(uv.xy * 100.0);
    float ny = noise(uv.yx * 100.0);
    
    float v = sin(nx * uTime + ny * uTime);
    c += v * .35f;
    outColor = c;
}