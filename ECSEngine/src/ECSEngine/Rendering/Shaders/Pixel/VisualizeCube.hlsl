#include "../Utilities.hlsli"

TextureCube environment_map;
SamplerState environment_sampler;

struct PS_INPUT
{
    float3 position : POSITION;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    float3 color = environment_map.Sample(environment_sampler, input.position);
    return float4(LinearToSRGB(color), 1.0f);
}