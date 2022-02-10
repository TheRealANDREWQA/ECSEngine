#include "../PBRBase.hlsli"
#include "../Utilities.hlsli"

struct PS_INPUT
{
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
};

cbuffer ConstantData : register(b0)
{
    float3 camera_position : packoffset(c0);
    float4 tint : packoffset(c1);
    float4 directional_light : packoffset(c2);
    float4 directional_color : packoffset(c3);
}

// Stripped down version of a PBR shader
float4 main(in PS_INPUT input) : SV_TARGET
{  
    float3 pixel_normal = normalize(input.normal);
    float3 view_direction = normalize(camera_position - input.world_position);
    
    float3 pixel_color = tint;
    float roughness = 0.5f;
    float ambient_occlusion = 1.0f;
    float metallic = 0.0f;
    
    float3 F0 = FresnelBaseReflectivity(pixel_color, metallic);
    float3 L0 = CalculateDirectionalLight(directional_light.xyz, view_direction, directional_color.rgb, pixel_color, pixel_normal, F0, metallic, roughness);
    
    // Select a base luminosity level for the mesh
    L0 += float3(0.3f, 0.3f, 0.3f);
    
    return float4(L0, 1.0f);
    //return float4(1.0f, 1.0f, 1.0f, 1.0f);
}