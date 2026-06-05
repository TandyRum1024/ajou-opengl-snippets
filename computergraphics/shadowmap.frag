/*
	Shadow mapping fragment shader
*/
#version 410 core
layout(location = 0) out vec4 out_Color;

void main(void)
{
	//out_Color = vec4(vec3(gl_FragCoord.z), 1.0);
	out_Color = vec4(vec3(gl_FragCoord.z), 1.0);
}
