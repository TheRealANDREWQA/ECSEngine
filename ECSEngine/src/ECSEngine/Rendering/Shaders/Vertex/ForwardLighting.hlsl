#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 object_matrix;
    float4x4 world_view_projection_matrix;
}

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float2 uv : TEXCOORD0;
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
    
    // Pass the UV coordinates
    output.uv = input.uv * float2(8.0f, 8.0f);
    
    // World space position for light calculation
    float3 world_space_position = mul(float4(input.position, 1.0f), object_matrix).xyz;
    
    output.world_position = world_space_position;
    
    return output;
}