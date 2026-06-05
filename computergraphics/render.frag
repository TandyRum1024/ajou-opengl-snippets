/*
	Basic Fragment shader
*/
#version 150 core

in vec4 vPos;
in vec4 vShadowmapPos;
in vec3 vNormal;
in vec2 vUV;
//in vec4 vCol;
out vec4 out_Color;

uniform vec3 uCameraPos;
uniform vec3 uLightPos;

uniform vec3 uLightColour;
uniform vec3 uSpecColour;
uniform vec3 uAmbColour;
uniform vec3 uColour;

uniform float uShininess;

uniform float uTime = 0.0;
uniform sampler2D sTexAlbedo;
uniform sampler2D sTexBump;
uniform sampler2D sTexShadowmap;
uniform bool uTextured = false;
uniform bool uDebugMode = false;

// Poisson disc samples from: https://www.geeks3d.com/20100628/3d-programming-ready-to-use-64-sample-poisson-disc/
const vec2 poissonOff[64] = vec2[] (
		vec2(-0.613392, 0.617481),
		vec2(0.170019, -0.040254),
		vec2(-0.299417, 0.791925),
		vec2(0.645680, 0.493210),
		vec2(-0.651784, 0.717887),
		vec2(0.421003, 0.027070),
		vec2(-0.817194, -0.271096),
		vec2(-0.705374, -0.668203),
		vec2(0.977050, -0.108615),
		vec2(0.063326, 0.142369),
		vec2(0.203528, 0.214331),
		vec2(-0.667531, 0.326090),
		vec2(-0.098422, -0.295755),
		vec2(-0.885922, 0.215369),
		vec2(0.566637, 0.605213),
		vec2(0.039766, -0.396100),
		vec2(0.751946, 0.453352),
		vec2(0.078707, -0.715323),
		vec2(-0.075838, -0.529344),
		vec2(0.724479, -0.580798),
		vec2(0.222999, -0.215125),
		vec2(-0.467574, -0.405438),
		vec2(-0.248268, -0.814753),
		vec2(0.354411, -0.887570),
		vec2(0.175817, 0.382366),
		vec2(0.487472, -0.063082),
		vec2(-0.084078, 0.898312),
		vec2(0.488876, -0.783441),
		vec2(0.470016, 0.217933),
		vec2(-0.696890, -0.549791),
		vec2(-0.149693, 0.605762),
		vec2(0.034211, 0.979980),
		vec2(0.503098, -0.308878),
		vec2(-0.016205, -0.872921),
		vec2(0.385784, -0.393902),
		vec2(-0.146886, -0.859249),
		vec2(0.643361, 0.164098),
		vec2(0.634388, -0.049471),
		vec2(-0.688894, 0.007843),
		vec2(0.464034, -0.188818),
		vec2(-0.440840, 0.137486),
		vec2(0.364483, 0.511704),
		vec2(0.034028, 0.325968),
		vec2(0.099094, -0.308023),
		vec2(0.693960, -0.366253),
		vec2(0.678884, -0.204688),
		vec2(0.001801, 0.780328),
		vec2(0.145177, -0.898984),
		vec2(0.062655, -0.611866),
		vec2(0.315226, -0.604297),
		vec2(-0.780145, 0.486251),
		vec2(-0.371868, 0.882138),
		vec2(0.200476, 0.494430),
		vec2(-0.494552, -0.711051),
		vec2(0.612476, 0.705252),
		vec2(-0.578845, -0.768792),
		vec2(-0.772454, -0.090976),
		vec2(0.504440, 0.372295),
		vec2(0.155736, 0.065157),
		vec2(0.391522, 0.849605),
		vec2(-0.620106, -0.328104),
		vec2(0.789239, -0.419965),
		vec2(-0.545396, 0.538133),
		vec2(-0.178564, -0.596057)
	);

mat3 getTBN (vec3 normal)
{
	vec3 q1 = dFdx(vPos.xyz), q2 = dFdy(vPos.xyz);
	vec2 uv1 = dFdx(vUV), uv2 = dFdy(vUV);
	float det = uv1.x*uv2.y - uv2.x*uv1.y;
	return mat3(
		normalize( (q1*uv2.y - q2*uv1.y) * det ),
		normalize( (-q1*uv2.x + q2*uv1.x) * det ),
		normal
	);
}

// Convert normalized 3D vector to equirectangular texture coordinates
// from: https://learnopengl.com/PBR/IBL/Diffuse-irradiance
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 convertSampleUV (vec3 v)
{
  vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
  uv *= invAtan;
  uv += 0.5;
  return uv;
}

// Fresnel equation approximator for the rim light
// from: https://learnopengl.com/PBR/Theory
vec3 fresnelSchlick(float dotv)
{
  // vec3 F0 = vec3(0.04);
  // map the specular shininess to base reflectivity
  vec3 F0 = vec3(smoothstep(1.0, 20.0, uShininess) * 0.04);
  return F0 + (1.0 - F0) * pow(clamp(1.0 - dotv, 0.0, 1.0), 5.0);
}

// Samples ambient light gradient for faux-global illumination 
// This will blend from grassy green colour into ambient light colour according to the normal
vec3 sampleAmbient (vec3 n)
{
	vec2 uv = convertSampleUV(n);
//	return mix(vec3(0.08, 0.15, 0.05), uAmbColour, uv.y);
	return mix(vec3(0.15, 0.29, 0.1), uAmbColour, uv.y);
	//return mix(vec3(0.0), vec3(1.0), uv.y);
}

// Pseudorandom generator from lecture note
float random(float i) {
    float dot_product = dot(vec4(gl_FragCoord.xyz,i), vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}


void main(void)
{
	vec3 	L = normalize(uLightPos - vPos.xyz),
			V = normalize(uCameraPos - vPos.xyz),
			N, R; // calculate those later
	// determine albedo later
	vec4 albedo;
	if (uTextured)
	{
		albedo = texture(sTexAlbedo, vUV);
		// Sample bumpmap
		const vec2 off = vec2(0.001, 0.0); // ?????????????
		// (calculte pseudoderivative)
		vec2 bumpDelta;
		bumpDelta = vec2(	texture(sTexBump, vUV + off.xy).r - texture(sTexBump, vUV - off.xy).r,
							texture(sTexBump, vUV + off.yx).r - texture(sTexBump, vUV - off.yx).r);
		vec3 bumpOff = vec3(bumpDelta.x * -4.0, bumpDelta.y * -4.0, 1.0);

		N = normalize(getTBN(vNormal) * bumpOff);
		R = 2.0 * dot(N, L) * N - L;
	}
	else
	{
		albedo = vec4(uColour, 1.0);
		N = normalize(vNormal);
		R = 2.0 * dot(N, L) * N - L;
	}
	
	// Calculate shadow intensity
	const float shadowBias = 0.001;
	float shadowFactor = 1.0;
	vec3 shadowPos = vShadowmapPos.xyz / vShadowmapPos.w * 0.5 + 0.5; // [uv, light depth]
	vec2 shadowUV = shadowPos.xy; // normalized into 0~1 range
	float depthLight = shadowPos.z;

	// Apply PCF / Dithering
	float shadowSize = (1.0 / 150);// * mix(1.0, 1.5, smoothstep(range.y, range.x, depthLight));
	const int ditheringSamples = 32;
	const float ditheringInvSamples = 1.0 / float(ditheringSamples);
	for (int i=0; i<ditheringSamples; i++)
	{
		// calculate 'dithered' offset
		// (modulate the offset with time, to achieve more random pattern. the dithering index also affects it but only when it has sampled more than 64th; the size of poisson disc table)
		float angle = random(uTime + (i/64)) * 3.14;
		mat2 rot = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
		vec2 uv = clamp(shadowUV + rot * poissonOff[i % 64] * shadowSize, 0.0, 1.0);
		float depthShadowMap = texture(sTexShadowmap, uv).r;

		// Shadow map test
		// Check if the UV is within the texture range 0..1 (override the shadowmap depth because it's "outdoors" light (AKA everything is 'lit' even outside the shadowmap bounds))
		//if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
		if (shadowUV.y >= 0 && shadowUV.y <= 1 &&
			depthLight > depthShadowMap + shadowBias)
		{
			//shadowFactor -= ditheringInvSamples * (1.0 - (length(off) / length(vec2(1.0, 1.0))));
			shadowFactor -= ditheringInvSamples;
		}
	}

	if (depthLight < 0 || depthLight > 1)
		shadowFactor = 1.0;

	shadowFactor = clamp(shadowFactor, 0.0, 1.0);

	// Darken up the shadow sample on the edge (vignette?)
	const float maxdistUV = distance(vec2(0.5), vec2(0.0));
	shadowFactor *= smoothstep(maxdistUV, 0.0, distance(vec2(0.5), shadowUV));
	
	// Calculate shading model
	vec3 diffuse = uLightColour * clamp(dot(N, L), 0.0, 1.0) * albedo.rgb;
	// Faux global illumination
	// (they won't notice a thing, belive me)
	vec3 ambient = albedo.rgb * sampleAmbient(N);
	// Fresnel
	// (inaccurate model, but looks good on my eyes)
	vec3 fresnel = fresnelSchlick(dot(N, V)) * max(0.1, dot(N, L));
	// Specular
	vec3 specColour = vec3(1.0);
	vec3 specular = specColour * pow(max(0.0, dot(R, V)), uShininess) * max(0.0, dot(N, L));

	//out_Color = vec4(sampleAmbient(N), 1.0);
	//out_Color = vec4(fresnel, 1.0);
	//out_Color = vec4(N, 1.0);
	//out_Color = vec4(vUV, 0.5, 1.0);
	//out_Color = albedo;
	//out_Color = vec4(vec3(depthShadowMap, clamp(depthShadowMap - depthLight, 0.0), clamp(depthLight - depthShadowMap, 0.0)), 1.0);
	//out_Color = vec4(vec3(0, max(depthShadowMap - depthLight, 0.0), max(depthLight - depthShadowMap, 0.0)), 1.0);
	out_Color = vec4(ambient + (diffuse + specular + fresnel) * shadowFactor, 1.0);

	//out_Color = vec4(vec3(fract(shadowUV), 0.5), 1.0);
	//out_Color = vec4(vec3(texture(sTexShadowmap, shadowUV).r), 1.0);
	if (uDebugMode)
		out_Color = vec4(vec3(texture(sTexShadowmap, shadowUV).r), 1.0);
}
