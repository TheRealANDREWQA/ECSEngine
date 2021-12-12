#include "../PBRBase.hlsli"

cbuffer SampleCount
{
    uint sample_count;
};

struct PS_INPUT
{
    float2 uv : UV;
};

float2 main(in PS_INPUT input) : SV_TARGET
{
    return IntegrateBRDF(input.uv.x, input.uv.y, sample_count);
    //return float2(1.0f, 1.0f);
}