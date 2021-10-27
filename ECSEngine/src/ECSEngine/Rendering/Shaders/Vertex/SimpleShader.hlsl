#include "../Macros.hlsli"

cbuffer Transform
{
    matrix transform;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

float4 main( in VS_INPUT input ) : SV_Position
{
    return mul(float4(input.position, 1.0f), transform);
}