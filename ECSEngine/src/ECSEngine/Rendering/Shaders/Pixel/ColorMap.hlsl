Texture2D color_map : register(t0);
SamplerState color_sampler : register(s0);

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : UV;
};

float4 main(in VS_OUTPUT input) : SV_Target0
{
    return color_map.Sample(color_sampler, input.uv);
}