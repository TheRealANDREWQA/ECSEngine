#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 object_matrix : packoffset(c0);
    float4x4 world_view_projection_matrix : packoffset(c4);
}

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VS_OUTPUT
{
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
    
    //The clip space position
    position = mul(float4(input.position, 1.0f), world_view_projection_matrix);
    
    // World space normal - the object is fixed, the normal is the same
    output.normal = mul(input.normal, (float3x3) object_matrix);
    
    // World space position for light calculation - the object is fixed; the position is the same
    output.world_position = mul(float4(input.position, 1.0f), object_matrix).xyz;;
    
	return output;
}