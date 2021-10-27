#include "../UIViewportTransform.hlsli"
#include "../Macros.hlsli"

cbuffer DrawRegionSize : register(b0)
{
    float2 region_center_position;
    float2 region_half_dimensions_inverse;
}

struct VS_INPUT
{
    float2 position : Position;
    float4 color : Color; ECS_REFLECT_UNORM_8
};

struct VS_OUTPUT 
{
	float4 position : SV_POSITION;
	float4 color : Color;
};

VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;
    float2 position_xy = TransformVertex(input.position.xy, region_center_position, region_half_dimensions_inverse);
	output.position = float4(position_xy, 0.001f, 1.0f);
	output.color = input.color;
	return output;
}