#include "../LightingUtilities.hlsli"

Texture2D ColorMap : register(t0);
SamplerState Sampler : register(s0);

cbuffer HemisphericConstants : register(b0) 
{
    float3 AmbientDown : packoffset(c0);
    float3 AmbientUp : packoffset(c1);
}

cbuffer DirectionalLightBuffer : register(b1)
{
    float3 DirectionalLightDirection : packoffset(c0);
    float3 DirectionalLightIntensityColor : packoffset(c1);
}

cbuffer SpecularFactors : register(b2)
{
    float SpecularExponent : packoffset(c0.x);
    float SpecularIntensity : packoffset(c0.y);
}

cbuffer PointLightBuffer : register(b3)
{
    float3 PointLightPosition : packoffset(c0);
    float PointLightRangeReciprocal : packoffset(c0.w);
    float3 PointLightColor : packoffset(c1);
    float PointLightAttenuationPower : packoffset(c1.w);
}

cbuffer SpotLightBuffer : register(b4)
{
    // Light Direction has to be normalized
    float3 SpotLightPosition : packoffset(c0);
    float SpotLightRangeReciprocal : packoffset(c0.w);
    float3 SpotLightDirection : packoffset(c1);
    float SpotLightCosineOuterCone : packoffset(c1.w);
    float3 SpotLightColor : packoffset(c2);
     // This is the difference between the inner cosine and the outer cosine
    float SpotLightCosineInnerConeReciprocal : packoffset(c2.w);
    float SpotLightConeAttenuation : packoffset(c3);
    float SpotLightRangeAttenuation : packoffset(c3.y);
}

cbuffer CapsuleLightBuffer : register(b5)
{
    // Direction must be normalized
    float3 CapsuleLightPosition : packoffset(c0);
    float CapsuleLightRangeReciprocal : packoffset(c0.w);
    float3 CapsuleLightDirection : packoffset(c1);
    float CapsuleLightLength : packoffset(c1.w);
    float3 CapsuleLightColor : packoffset(c2);
    float CapsuleLightAttenuation : packoffset(c2.w);
}

struct PS_INPUT
{
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
    float3 view_direction : VIEW_DIRECTION;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    float4 albedo = ColorMap.Sample(Sampler, input.uv);
    float3 normalized_normal = normalize(input.normal);
    
    float3 light_direction = DirectionalLightDirection;
    
    float3 normalized_light_direction = normalize(light_direction);
    float3 normalized_view_direction = normalize(input.view_direction);
    
    float3 ambient_color = CalculateAmbient(normalized_normal, albedo.xyz, AmbientDown, AmbientUp);
    float3 directional_color = CalculateBlinnPhong(
        normalized_normal,
        normalized_view_direction,
        normalized_light_direction,
        SpecularExponent,
        SpecularIntensity,
        DirectionalLightIntensityColor,
        albedo.rgb
    );
    
    float3 point_light = CalculatePointLight(
        normalized_normal,
        normalized_view_direction,
        input.world_position,
        albedo.xyz,
        PointLightPosition,
        PointLightColor,
        PointLightRangeReciprocal,
        PointLightAttenuationPower,
        SpecularExponent,
        SpecularIntensity
    );
    
    float3 spot_light = CalculateSpotLight(
        normalized_normal,
        normalized_view_direction,
        input.world_position,
        albedo.rgb,
        SpotLightPosition,
        SpotLightDirection,
        SpotLightColor,
        SpotLightRangeReciprocal,
        SpotLightCosineOuterCone,
        SpotLightCosineInnerConeReciprocal,
        SpotLightConeAttenuation,
        SpotLightRangeAttenuation,
        SpecularExponent,
        SpecularIntensity
    );
    
    float3 capsule_light = CalculateCapsuleLight(
        normalized_normal,
        normalized_view_direction,
        input.world_position,
        albedo.rgb,
        CapsuleLightPosition,
        CapsuleLightDirection,
        CapsuleLightColor,
        CapsuleLightRangeReciprocal,
        CapsuleLightLength,
        CapsuleLightAttenuation,
        SpecularExponent,
        SpecularIntensity
    );
    
    return float4(ambient_color /*+ directional_color*/ /*+ point_light*/ + spot_light /*+ capsule_light*/, 1.0f);
}