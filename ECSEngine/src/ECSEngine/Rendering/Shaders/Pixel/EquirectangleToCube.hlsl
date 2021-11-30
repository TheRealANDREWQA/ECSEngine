#include "../Utilities.hlsli"

Texture2D equirectangular_map;
SamplerState map_sampler;

struct PS_INPUT
{
    float3 position : POSITION;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    float2 uv = SampleSphericalMap(normalize(input.position));
    return float4(equirectangular_map.Sample(map_sampler, uv).rgb, 1.0f);
}