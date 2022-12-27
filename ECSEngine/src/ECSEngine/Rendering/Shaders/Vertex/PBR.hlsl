#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 object_matrix : packoffset(c0);
    float4x4 world_view_projection_matrix : packoffset(c4);
    float2 uv_tiling : packoffset(c8.x);
    float2 uv_offset : packoffset(c8.z);
    float2 new_value : packoffset(c9.x);
    float4x4 new_tiled_value : packoffset(c10.x);
}

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
};

struct VS_OUTPUT
{
    float2 uv : UV;
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
};
    
VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
    
    //The clip space position
    position = mul(float4(input.position, 1.0f), world_view_projection_matrix);
    
    // World space normal
    output.normal = mul(input.normal, (float3x3) object_matrix);
    
    // Pass the UV coordinates modified according to the parameters
    output.uv = input.uv * uv_tiling + uv_offset;
    
    // World space position for light calculation
    float3 world_space_position = mul(float4(input.position, 1.0f), object_matrix).xyz;
    
    output.world_position = world_space_position;
    
    return output;
}