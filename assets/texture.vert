uniform mat4 un_ProjectionMatrix;

attribute vec2 in_vertex;
attribute vec2 in_texcoord;

varying vec2 ex_texcoord;

//gl_Position = gl_ModelViewMatrix * gl_ProjectionMatrix * gl_Vertex; //Trivial vertex shader

void main(void)
{
	gl_Position = un_ProjectionMatrix * vec4(in_vertex, 0, 1);

	ex_texcoord = in_texcoord;
}
