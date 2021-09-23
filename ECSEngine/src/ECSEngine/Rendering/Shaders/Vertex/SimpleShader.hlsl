#include "../Macros.hlsli"

cbuffer Transform
{
    matrix transform;
};

struct VS_INPUT
{
    float3 position : POSITION; ECS_REFLECT_FORMAT(DXGI_FORMAT_R32G32B32_FLOAT)
};

float4 main( in VS_INPUT input ) : SV_Position
{
    return mul(float4(input.position, 1.0f), transform);
}