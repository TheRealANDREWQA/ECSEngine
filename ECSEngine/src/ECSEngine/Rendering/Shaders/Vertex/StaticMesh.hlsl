#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 object_matrix;
    float4x4 world_view_projection_matrix;
}

cbuffer Lighting : register(b1)
{
    float3 light_position;
}

struct VS_INPUT
{
    float3 position : POSITION; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT)
    float2 uv : TEXCOORD; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32_FLOAT)
    float3 normal   : NORMAL; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT)
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 light    : LIGHT;
};
    
VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;
    
    // World space position for light calculation
    float3 world_space_position = mul(float4(input.position, 1.0f), object_matrix).xyz;
    
    // The clip space position
    //output.position = mul(float4(input.position, 1.0f), world_view_projection_matrix);
    
    output.position = float4(input.position, 1.0f);
    
    // Adjuted normal
    output.normal = mul(input.normal, (float3x3)object_matrix);

    // Light direction
    output.light = light_position - world_space_position;
    
    // Pass the UV coordinates
    output.uv = input.uv;
    
    return output;
}