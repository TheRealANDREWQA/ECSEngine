#include "../Macros.hlsli"

cbuffer Matrices : register(b0)
{
    float4x4 world_view_projection_matrix : packoffset(c4);
};

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
    
    position = mul(float4(input.position, 1.0f), world_view_projection_matrix);
    
    return output;
}