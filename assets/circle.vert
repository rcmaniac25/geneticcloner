uniform vec2 un_position;
uniform float un_scale;
uniform mat4 un_ProjectionMatrix;

attribute vec2 in_vertex;

//gl_Position = gl_ModelViewMatrix * gl_ProjectionMatrix * gl_Vertex; //Trivial vertex shader

void main(void)
{
	//scale * translationToCertainPoint * rotation * translationToObjectPosition
	mat4 scale = mat4(un_scale);
	scale[3][3] = 1.0;
	
	mat4 trans = mat4(1.0);
	trans[3] = vec4(un_position, 0, 1);
	
	mat4 modelView = scale * trans;
	
	gl_Position = modelView * un_ProjectionMatrix * vec4(in_vertex, 0, 1);
}
