#version 450

const uint DRAW_FLAG_USE_ALPHA_MAP = 1U << 0U;
const uint DRAW_FLAG_LINEAR_SAMPLING = 1U << 1U;
const uint DRAW_FLAG_ALPHA_RED_CHANNEL = 1U << 2U;
const uint DRAW_FLAG_USE_EDGE_BLEND = 1U << 3U;
const uint DRAW_FLAG_SMOOTHSTEP_EDGE_BLEND = 1U << 4U;

layout( set = 0, binding = 0 ) uniform sampler2D source_texture;
layout( set = 0, binding = 1 ) uniform sampler2D alpha_texture;

layout( push_constant ) uniform DrawParameters
{
    uint draw_flags;
    float draw_opacity;
    float edge_blend_left;
    float edge_blend_right;
    float edge_blend_top;
    float edge_blend_bottom;
} parameters;

layout( location = 0 ) in vec2 input_uv;
layout( location = 0 ) out vec4 output_color;

float applyEdgeCurve( float value )
{
    const float normalized = clamp( value, 0.0, 1.0 );
    return ( parameters.draw_flags & DRAW_FLAG_SMOOTHSTEP_EDGE_BLEND ) != 0U
        ? normalized * normalized * ( 3.0 - 2.0 * normalized )
        : normalized;
}

void main()
{
    vec4 source_color;
    vec4 alpha_color;
    if( ( parameters.draw_flags & DRAW_FLAG_LINEAR_SAMPLING ) != 0U )
    {
        source_color = texture( source_texture, input_uv );
        alpha_color = texture( alpha_texture, input_uv );
    }
    else
    {
        source_color = texture( source_texture, input_uv );
        alpha_color = texture( alpha_texture, input_uv );
    }

    const float alpha_sample =
        ( parameters.draw_flags & DRAW_FLAG_ALPHA_RED_CHANNEL ) != 0U
            ? alpha_color.r : alpha_color.a;
    const float alpha_factor =
        ( parameters.draw_flags & DRAW_FLAG_USE_ALPHA_MAP ) != 0U
            ? alpha_sample : 1.0;
    float edge_factor = 1.0;
    if( ( parameters.draw_flags & DRAW_FLAG_USE_EDGE_BLEND ) != 0U )
    {
        if( parameters.edge_blend_left > 0.0 )
        {
            edge_factor *= applyEdgeCurve( input_uv.x / parameters.edge_blend_left );
        }
        if( parameters.edge_blend_right > 0.0 )
        {
            edge_factor *= applyEdgeCurve(
                ( 1.0 - input_uv.x ) / parameters.edge_blend_right );
        }
        if( parameters.edge_blend_top > 0.0 )
        {
            edge_factor *= applyEdgeCurve( input_uv.y / parameters.edge_blend_top );
        }
        if( parameters.edge_blend_bottom > 0.0 )
        {
            edge_factor *= applyEdgeCurve(
                ( 1.0 - input_uv.y ) / parameters.edge_blend_bottom );
        }
    }
    source_color.a *= alpha_factor * edge_factor * parameters.draw_opacity;
    output_color = source_color;
}
