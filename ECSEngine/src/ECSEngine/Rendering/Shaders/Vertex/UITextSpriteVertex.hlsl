#include "../UIViewportTransform.hlsli"
#include "../Macros.hlsli"

cbuffer DrawRegionSize : register(b0)
{
    float2 region_center_position;
    float2 region_half_dimensions_inverse;
}

struct VS_INPUT 
{
    float2 position : Position; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32_FLOAT)
    float2 uv : UV; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32_FLOAT) 
    float4 color : Color; ECS_REFLECT_FORMAT(DXGI_FORMAT_R8G8B8A8_UNORM) 
};

struct VS_OUTPUT 
{
	float4 position : SV_POSITION;
	float4 color : Color;
	float2 uv : TEXCOORD0;
};

VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;
    float2 position_xy = TransformVertex(input.position.xy, region_center_position, region_half_dimensions_inverse);
    output.position = float4(position_xy, 0.001f, 1.0f);
	output.color = input.color;
	output.uv = input.uv;
	return output;
}