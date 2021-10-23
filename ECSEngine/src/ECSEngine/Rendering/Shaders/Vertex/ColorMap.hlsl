#include "../Macros.hlsli"

cbuffer Transform
{
    matrix transform;
};

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT) 
    float2 uv : UV; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32_FLOAT)
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : UV;
};

VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.uv = input.uv * float2(10.0f, 5.0f);
    return output;
}