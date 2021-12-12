#include "../Macros.hlsli"

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float2 uv : UV;
};

struct VS_OUTPUT
{
    float2 uv : UV;
};

VS_OUTPUT main( in VS_INPUT input, out float4 clip_position : SV_Position )
{
    VS_OUTPUT output;
    clip_position = float4(input.position, 1.0f);
    output.uv = input.uv;
    
    return output;
}