/*
	Shadow mapping vertex shader
*/
#version 410 core

layout(location=0) in vec3 in_Position;

// Transform/MVP matrix
uniform mat4 MAT_MVP = mat4(	1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0 );

void main(void)
{
	vec4 pos = MAT_MVP * vec4(in_Position.xyz, 1.0);
	gl_Position = pos;
}