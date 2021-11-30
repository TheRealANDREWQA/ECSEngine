#include "../PBRBase.hlsli"
#include "../Utilities.hlsli"

Texture2D ColorMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MetallicMap : register(t2);
Texture2D RoughnessMap : register(t3);
Texture2D AmbientOcclusion : register(t4);
SamplerState Sampler : register(s0);

TextureCube environment_diffuse : register(t5);

struct PS_INPUT
{
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
};

cbuffer CameraPosition : register(b0)
{
    float3 camera_position;
};

cbuffer PointLights : register(b1)
{
    float4 light_positions[4];
    float4 light_colors[4];
    float light_range[4];
};

cbuffer FlatValues : register(b2)
{
    float metallic_factor;
    float roughness_factor;
}

cbuffer DirectionalLight : register(b3)
{
    float4 directional_light;
    float4 directional_color;
}

float4 main(in PS_INPUT input) : SV_TARGET
{
    float3 normal_map_sample = NormalMap.Sample(Sampler, input.uv);
    float3 pixel_normal = ComputePixelNormal(normal_map_sample, input.world_position, input.normal, input.uv, 1.0f);
    float3 view_direction = normalize(camera_position - input.world_position);
    float3 pixel_color = SRGBToLinear(ColorMap.Sample(Sampler, input.uv).rgb);
    float roughness = RoughnessMap.Sample(Sampler, input.uv).r * roughness_factor;
    float ambient_occlusion = AmbientOcclusion.Sample(Sampler, input.uv);
    //float metallic = MetallicMap.Sample(Sampler, input.uv).r * metallic_factor;
    float metallic = metallic_factor;
    float3 F0 = FresnelBaseReflectivity(pixel_color, metallic);
    
    uint index = 0;
    float3 L0 = float3(0.0f, 0.0f, 0.0f);
    for (; index < 4; index++)
    {
        float3 light_direction = normalize(light_positions[index].xyz - input.world_position);
        float3 halfway = normalize(light_direction + view_direction);
        
        float distance = length(light_positions[index].xyz - input.world_position);
        float attenuation = PointLightAttenuation(distance, light_range[index]);
        
        float3 radiance = light_colors[index].rgb * attenuation;
        
        L0 += CalculateBRDFDirectLight(radiance, pixel_color, pixel_normal, view_direction, light_direction, halfway, F0, metallic, roughness);
    }
    float3 light_direction = normalize(directional_light.xyz);
    float3 halfway = normalize(light_direction + view_direction);
    float3 radiance = directional_color.rgb;
    L0 += CalculateBRDFDirectLight(radiance, pixel_color, pixel_normal, view_direction, light_direction, halfway, F0, metallic, roughness);
    
    float3 ambient_diffuse = environment_diffuse.Sample(Sampler, normal_map_sample).rgb;
    ambient_diffuse = CalculateAmbientDiffuse(ambient_diffuse, pixel_color, normal_map_sample, view_direction, F0, roughness) * ambient_occlusion;
    L0 += ambient_diffuse;
    L0 = TonemapACESApproximation(L0);
    
    return float4(L0, 1.0f);
}