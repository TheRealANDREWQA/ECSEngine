#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 object_matrix;
    float4x4 world_view_projection_matrix;
}

cbuffer CameraPosition : register(b1)
{
    float3 CameraPosition;
}

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct VS_OUTPUT
{
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
    float3 view_direction : VIEW_DIRECTION;
};
    
VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
    
    //The clip space position
    position = mul(float4(input.position, 1.0f), world_view_projection_matrix);
    
    // World space normal
    output.normal = mul(input.normal, (float3x3) object_matrix);
    
    // Pass the UV coordinates
    output.uv = input.uv * float2(10.0f, 5.5f);
    
    // World space position for light calculation
    float3 world_space_position = mul(float4(input.position, 1.0f), object_matrix).xyz;
    
    // View direction
    output.view_direction = CameraPosition - world_space_position;
    output.world_position = world_space_position;
    
    return output;
}