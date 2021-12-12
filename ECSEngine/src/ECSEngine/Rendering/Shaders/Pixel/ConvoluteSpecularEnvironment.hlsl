#include "../Utilities.hlsli"
#include "../PBRBase.hlsli"

TextureCube environment_map;
SamplerState map_sampler;

cbuffer SampleCount : register(b0)
{
    uint sample_count;
};

cbuffer Roughness : register(b1)
{
    float roughness;
};

cbuffer MainFaceResoultion : register(b2)
{
    float resolution;
}

struct PS_INPUT
{
    float3 position : POSITION;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.position);
    // The normal y direction must be inverted, otherwise the image will be reversed
    normal.y = -normal.y;
    float3 view_direction = normal;
    
    float total_weight = 0.0f;
    float3 irradiance = 0.0f;
    for (uint index = 0; index < sample_count; index++)
    {
        float2 xi = Hammersley(index, sample_count);
        float3 halfway = ImportanceSampleGGX(xi, normal, roughness);
        float3 light_direction = normalize(-reflect(halfway, normal));
        
        float n_dot_l = ClampedDot(normal, light_direction);
        // If n_dot_l is 0.0f, it will do nothing 
        // Maybe worth testing to see if an if here to avoid a texture sample is worth the divergence cost
        
        if (n_dot_l > 0.0f)
        { 
            float n_dot_h = ClampedDot(normal, halfway);
            float h_dot_v = ClampedDot(halfway, view_direction);
            float distribution = DistributionFunction(normal, halfway, roughness);
            float pdf = (distribution * n_dot_h / (4.0 * h_dot_v)) + 0.0001;

            float texel_weight = 4.0 * PI / (6.0 * resolution * resolution);
            float sample_weight = 1.0 / (float(sample_count) * pdf + 0.0001);

            float mip_level = roughness == 0.0 ? 0.0 : 0.5 * log2(sample_weight / texel_weight);
        
            irradiance += environment_map.SampleLevel(map_sampler, light_direction, mip_level).rgb * n_dot_l;
            total_weight += n_dot_l;
        }
    }
    irradiance /= total_weight;
    
    return float4(irradiance, 1.0f);
}