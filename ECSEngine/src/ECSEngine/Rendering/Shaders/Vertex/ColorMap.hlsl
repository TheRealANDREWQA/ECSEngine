#include "../Macros.hlsli"

cbuffer Transform
{
    matrix transform;
};

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float2 uv : UV;
};

struct VS_OUTPUT
{
    float4 position : SV_position;
    float2 uv : UV;
};

VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.uv = input.uv * float2(10.0f, 5.0f);
    return output;
}