#version 450

layout(location = 0) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D img;

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

layout(push_constant) uniform PushConstants
{
    ivec2 resolution;
    ivec2 simResolution;
    float time;
    float pixelation;
    float p1Lerp;
    float p2Lerp;
} pushConstants;

float applyVignette(vec2 uv)
{
    uv *=  1.0 - uv.yx;
    float vig = uv.x*uv.y * 15.0;
    vig = pow(vig, 0.45);
    return vig;
}

vec2 Dist(vec2 pos, vec2 res)
{
	pos = pos * res;
	pos -= floor(pos);
	pos -= vec2(.5);
	pos = abs(pos);
	pos.xy = vec2(max(pos.x, pos.y));
	return pos;
}

float GetLightLerped(vec2 uv, float org, float lerp)
{
    uv.y += pow(abs(sin(uv.x + pushConstants.time)), 4) * .2f;
    float rDis = 1.f - abs(uv.y - org);
    return rDis * lerp;
}

void main()
{
    vec2 scaledRes = pushConstants.simResolution;
    while(scaledRes.x + pushConstants.simResolution.x <= pushConstants.resolution.x && 
        scaledRes.y + pushConstants.simResolution.y <= pushConstants.resolution.y)
        scaledRes += pushConstants.simResolution;
    vec2 scaledV = pushConstants.resolution / scaledRes;
    vec2 scaledFragPos = fragPos * scaledV;

    vec2 sRf = pushConstants.simResolution;
    vec2 scaledOff = (pushConstants.resolution - scaledRes) / 2;
    scaledOff = scaledOff / pushConstants.resolution;
    scaledOff.x *= sRf.x / sRf.y;
    scaledFragPos -= scaledOff;

    float m = pushConstants.simResolution.x / pushConstants.simResolution.y;
    vec2 res = vec2(pushConstants.simResolution.x * m, pushConstants.simResolution.y);
    vec2 uv = scaledFragPos;
    
    vec2 d = Dist(uv, res);
    float b = length(d);
    b = step(.45, b);

    uv *= res;
    uv = floor(uv);
    uv /= res;
    uv += vec2(0.5f / pushConstants.simResolution);

    vec2 pixRes = vec2(pushConstants.simResolution.x * m / pushConstants.pixelation, pushConstants.simResolution.y / pushConstants.pixelation);
    vec2 pixUv = scaledFragPos;
    pixUv *= pixRes;
    pixUv = floor(pixUv);
    pixUv /= pixRes;
    pixUv += vec2(0.5f / pushConstants.simResolution);
    
    float nx = noise(uv.xy * pushConstants.simResolution.x);
    float ny = noise(uv.yx * pushConstants.simResolution.y);
    
    float v = sin(nx * pushConstants.time + ny * pushConstants.time);
    v *= .06f;

    vec4 bgl = vec4(1, 0, 0, 1) * GetLightLerped(uv, 1, pushConstants.p1Lerp) + vec4(0, 1, 0, 1) * GetLightLerped(uv, 1, pushConstants.p2Lerp);
    bgl = v > .02 ? bgl : vec4(0);

    vec4 color = texture(img, pixUv);
    vec4 bgColor = vec4(vec3(v), 1.0);
    bgColor += pow(bgl, vec4(6));
    vec4 mixed = mix(color, bgColor, 1.f - color.a);
    vec4 sub = vec4(vec3(b), 0.0);
    vec4 f = mixed - sub;
    f = max(f, color * .25 * color.a);
    outColor = f * applyVignette(uv);
}