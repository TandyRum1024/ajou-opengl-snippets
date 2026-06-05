/*
	Basic Vertex shader
*/
#version 410 core

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Normal;
layout(location=2) in vec2 in_UV;
layout(location=3) in vec4 in_Colour;

// Transform/MVP matrix
// (model)
uniform mat4 MAT_M = mat4(	1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0 );
// (view)
uniform mat4 MAT_V = mat4(	1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0 );
// (projection)
uniform mat4 MAT_P = mat4(	1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0 );
// (light)
uniform mat4 MAT_LIGHT = mat4(	1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0 );

// Stuffs that gets sent to fragment shader
out vec4 vPos;
out vec4 vShadowmapPos;
out vec3 vNormal;
out vec2 vUV;
//out vec4 vCol;

void main(void)
{
	vec3 normal = normalize(MAT_M * vec4(in_Normal, 0.0)).xyz;
	vec4 pos = MAT_P * MAT_V * MAT_M * vec4(in_Position.xyz, 1.0);

	// Send to fragment
	vPos = MAT_M * vec4(in_Position.xyz, 1.0);
	vShadowmapPos = MAT_LIGHT * vec4(in_Position.xyz, 1.0);
	vNormal = normal;
	vUV = in_UV;
	//vCol = in_Colour;
	gl_Position = pos;
}