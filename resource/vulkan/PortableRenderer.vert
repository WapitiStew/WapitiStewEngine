#version 450

layout( location = 0 ) in vec2 input_position;
layout( location = 1 ) in vec2 input_uv;

layout( location = 0 ) out vec2 output_uv;

void main()
{
    gl_Position = vec4( input_position, 0.0, 1.0 );
    output_uv = input_uv;
}
