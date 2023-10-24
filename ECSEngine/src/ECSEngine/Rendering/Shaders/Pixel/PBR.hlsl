#include "../Macros.hlsli"
#include "../PBRBase.hlsli"
#include "../Utilities.hlsli"
#include "../CBufferTags.hlsli"

#ifdef ENVIRONMENT_TEXTURE
TextureCube environment_diffuse : register(t0);
TextureCube environment_specular : register(t1);
Texture2D<float2> brdf_lut : register(t2);
SamplerState environment_sampler : register(s0);
#endif

#ifdef COLOR_TEXTURE
Texture2D ColorMap : register(t3);
#endif

#ifdef NORMAL_TEXTURE
Texture2D NormalMap : register(t4);
#endif

#ifdef METALLIC_TEXTURE
Texture2D MetallicMap : register(t5);
#endif

#ifdef ROUGHNESS_TEXTURE
Texture2D RoughnessMap : register(t6);
#endif

#ifdef OCCLUSION_TEXTURE
Texture2D AmbientOcclusion : register(t7);
#endif


SamplerState pbr_texture_sampler : register(s1);

struct PS_INPUT
{
    float2 uv : UV;
    float3 normal : NORMAL;
    float3 world_position : WORLD_POSITION;
};

cbuffer CameraPosition : register(b0)
{
    float3 camera_position; ECS_INJECT_CAMERA_POSITION
};

#ifdef ENVIRONMENT_TEXTURE

cbuffer Environment : register(b1)
{
    float environment_specular_max_mip : packoffset(c0.x);
    float environment_diffuse_factor : packoffset(c0.y);
    float environment_specular_factor : packoffset(c0.z);
};

#endif

cbuffer Modifiers : register(b2)
{
    float4 tint : packoffset(c0); ECS_REFLECT_AS_COLOR
    float metallic_factor : packoffset(c1.x); ECS_REFLECT_DEFAULT(1.0)
    float roughness_factor : packoffset(c1.y); ECS_REFLECT_DEFAULT(1.0)
    float normal_strength : packoffset(c1.z); ECS_REFLECT_DEFAULT(1.0)
    float ambient_occlusion_factor : packoffset(c1.w);
    float3 emissive_factor : packoffset(c2);
    //float4 another_tint : packoffset(c3); ECS_REFLECT_AS_COLOR
}

#ifdef POINT_LIGHTS

cbuffer PointLights : register(b3)
{
    float4 light_positions[4];
    float4 light_colors[4];
    float light_range[4];
};

#endif


cbuffer DirectionalLight : register(b4)
{
    float3 directional_light;
    float4 directional_color; ECS_REFLECT_AS_FLOAT_COLOR
}

float4 main(in PS_INPUT input) : SV_TARGET
{
    float2 uv_derivatives_x = ddx(input.uv);
    float2 uv_derivatives_y = ddy(input.uv);
    
    #ifdef NORMAL_TEXTURE
    float3 normal_map_sample = NormalMap.SampleGrad(pbr_texture_sampler, input.uv, uv_derivatives_x, uv_derivatives_y);
    float3 pixel_normal = ComputePixelNormal(normal_map_sample, input.world_position, input.normal, input.uv, normal_strength);
    #else
    float3 pixel_normal = normalize(input.normal);
    #endif
    
    float3 view_direction = normalize(camera_position - input.world_position);
    
    #ifdef COLOR_TEXTURE
    //float3 pixel_color = SRGBToLinear(ColorMap.SampleGrad(pbr_texture_sampler, input.uv, uv_derivatives_x, uv_derivatives_y).rgb) * tint;
    float3 pixel_color = ColorMap.SampleGrad(pbr_texture_sampler, input.uv, uv_derivatives_x, uv_derivatives_y).rgb * tint;
    #else 
    float3 pixel_color = tint;
    #endif
    
    #ifdef ROUGHNESS_TEXTURE
    float roughness = RoughnessMap.SampleGrad(pbr_texture_sampler, input.uv, uv_derivatives_x, uv_derivatives_y).r * roughness_factor;
    #else
    float roughness = roughness_factor;
    #endif
    
    #ifdef OCCLUSION_TEXTURE
    float ambient_occlusion = AmbientOcclusion.SampleGrad(pbr_texture_sampler, input.uv, uv_derivatives_x, uv_derivatives_y).r * ambient_occlusion_factor;
    #else
    float ambient_occlusion = ambient_occlusion_factor;
    #endif
    
    #ifdef METALLIC_TEXTURE
    float metallic = MetallicMap.SampleGrad(pbr_texture_sampler, input.uv, uv_derivatives_x, uv_derivatives_y).r * metallic_factor;
    #else
    float metallic = metallic_factor;
    #endif
    
    float3 F0 = FresnelBaseReflectivity(pixel_color, metallic);
    
    float3 L0 = float3(0.0f, 0.0f, 0.0f);
    
    #ifdef POINT_LIGHTS
    uint index = 0;
    // The total radiance
    for (; index < 4; index++)
    {
        L0 += CalculatePointLight(light_positions[index].xyz, light_colors[index].rgb, light_range[index], input.world_position, pixel_color,
        pixel_normal, view_direction, F0, metallic, roughness);
    }
    #endif
    L0 += CalculateDirectionalLight(directional_light, view_direction, directional_color, pixel_color, pixel_normal, F0, metallic, roughness);
    
    #ifdef ENVIRONMENT_TEXTURE
    float3 ambient = CalculateAmbient(view_direction, pixel_normal, pixel_color, F0, roughness, metallic, environment_diffuse, environment_specular,
    brdf_lut, environment_sampler, environment_specular_max_mip, environment_diffuse_factor, environment_specular_factor);
    L0 += ambient * ambient_occlusion;
    #endif
    
    //L0 = TonemapLottes(L0);
    
    return float4(LinearToSRGB(L0), 1.0f);
}