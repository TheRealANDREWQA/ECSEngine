#include "../Macros.hlsli"

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float2 uv : UV;
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
    
    position = float4(input.position, 1.0f);
    output.uv = float2((input.position.x + 1.0f) * 0.5f, 1.0f - (input.position.y + 1.0f) * 0.5f);
    
    return output;
}