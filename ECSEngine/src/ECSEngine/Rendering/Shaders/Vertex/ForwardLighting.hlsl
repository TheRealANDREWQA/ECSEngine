#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 object_matrix;
    float4x4 world_view_projection_matrix;
}

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT) 
    float2 uv : TEXCOORD; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32_FLOAT) 
    float3 normal : NORMAL; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT) 
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
};
    
VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;
    
    //The clip space position
    output.position = mul(float4(input.position, 1.0f), world_view_projection_matrix);
    
    //output.position = float4(input.position, 1.0f);
    
    // World space normal
    output.normal = mul(input.normal, (float3x3) object_matrix);
    
    // Pass the UV coordinates
    output.uv = input.uv * float2(10.0f, 5.5f);
    
    // World space position for light calculation
    float3 world_space_position = mul(float4(input.position, 1.0f), object_matrix).xyz;
    
    //// Light direction
    //output.light_direction = world_space_position - DirectionalLightDirection;
    
    //// View direction
    //output.view_direction = world_space_position - CameraPosition;
    output.world_position = world_space_position;
    
    return output;
}