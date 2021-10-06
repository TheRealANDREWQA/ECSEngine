Texture2D ColorMap : register(t0);
SamplerState Sampler : register(s0);

RWTexture2D<float4> ColoMap;

cbuffer LightColor
{
    float4 color;
};

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 light    : LIGHT;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    //float3 normalized_normal = normalize(input.normal);
    //float3 normalized_light_direction = normalize(input.light);
    
    //float4 illumination = max(dot(normalized_normal, normalized_light_direction), 0.0f) + 0.2f;
    //float4 surface_color = ColorMap.Sample(Sampler, input.uv);
    
    //return surface_color * illumination;
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}