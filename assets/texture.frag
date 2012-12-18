//Simply copy the texture
precision mediump float;

uniform sampler2D un_texture;

varying vec2 ex_texcoord;

void main (void)
{
	gl_FragColor = texture2D(un_texture, ex_texcoord);
}
