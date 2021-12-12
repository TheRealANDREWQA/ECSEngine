#ifndef __PBR__
#define __PBR__

#include "Utilities.hlsli"

/* This file contains all the functions related to the PBR rendering
*  Credit must be given to https://learnopengl.com/PBR/Theory for all the information presented here
*  alongside all the researchers and persons involved in developing the PBR theory.
*  Worth mentioning is also the Unreal Engine 4 team that filtered the BRDF functions for use in realtime.
*  Presentation at https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
*  by Brian Karis.
*
*
*  w - small omega - the solid angle of the area or volume projected on the unit sphere
*  L - the radiance
*  L0  measures the reflected sum of the lights' irradiance onto point p as viewed from w0.
*  a * b means the dot product of the vectors a and b
* 
*  The reflectance equation: L0(p, w0) = integral { fr(p, wi, w0) Li(p, wi) n * wi dwi } over a hemisphere
*  fr - The BRDF - bidirectional reflective distribution function
*  fr inputs - incoming light direction wi, outgoing light direction w0, the surface normal n and the roughness a
* 
*  These functions will use the Cook-Torrance BRDF
*  fr = kd * f(lambert) + ks * f(cook-torrance)
*  kd is the ratio of incoming light energy that gets refracted with ks being the ratio that gets reflected
*
*  f(lambert) = c / PI; c - the "albedo" or surface color; it is divided by PI because it is integrated over a hemisphere
*  f(cook-torrance) = (DFG) / (4 (w0 * n)(wi * n))
*  D - Distribution function: approximates the amount of the surface's microfacets that are aligned to the halfway vector, 
*  influenced by the roughness of the surface; this is the primary function approximating the microfacets.
*  F - Fresnel equation: The Fresnel equation describes the ratio of surface reflection at different surface angles.
*  G - Geometry function: describes the self-shadowing property of the microfacets. When a surface is relatively rough, 
*  the surface's microfacets can overshadow other microfacets reducing the light the surface reflects.
*
*  The functions chosen are:
*  D - Trowbridge-Reitz GGX
*  F - Fresnel-Schlick approximation
*  G - Smith's Schlick-GGX
*
*  Normal distribution function
*  The normal distribution function D statistically approximates the relative surface area of microfacets exactly aligned to the (halfway) vector h. 
*  Trowbridge-Reitz GGX:
*  D(n, h, a) = (a ^ 2) / {PI * [((n * h) ^ 2)(a ^ 2 - 1) + 1] ^ 2} ; h is the halfway vector to measure against the surface's microfacets
* 
*  Geometry function
*  The geometry function statistically approximates the relative surface area where its micro surface-details overshadow each other, 
*  causing light rays to be occluded.
*  Schlick-GGX:
*  Gs(n, v, k) = (n * v) / ((n * v)(1 - k) + k) ; k is a remapping of the roughness for different lighting conditions
*  k = ((a + 1) ^ 2) / 8 for direct lighting
*  k = (a ^ 2) / 2 for image based lighting or IBL for short
*
*  To effectively approximate the geometry we need to take account of both the view direction (geometry obstruction) and 
*  the light direction vector (geometry shadowing). We can take both into account using Smith's method:
*  G(n, v, l, k) = Gs(n, v, k) * Gs(n, l, k)
*
*  Fresnel equation
*  Fresnel-Schlick approximation:
*  F(h, v, F0) = F0 + (1 - F0)(1 - (h * v)) ^ 5
*  F0 represents the base reflectivity of the surface, which we calculate using something called the indices of refraction or IOR only for dielectrics.
*  F0 for conductors (metals) is it the surface's response at normal incidence (0 degree angle).
*  By pre-computing F0 for both dielectrics and conductors we can use the same Fresnel-Schlick approximation for both types of surfaces,
*  but we do have to tint the base reflectivity if we have a metallic surface. We generally accomplish this as follows lerping based on a metalness value.
*  F0 for dielectrics is usually averaged around 0.04.
*
*  Given the fact that f(cook-torrance) already takes into account the ratio of light that gets reflected on a surface by the F term, the ks into the
*  fr function is not necessary.
*
*  This workflow uses the following texture maps:
*  Albedo/Color - with no light interraction (ideally, the same value across all pixels).
*  Normal.
*  Metallic.
*  Roughness.
*  (Optional) Ambient Occlusion.
*
*
*
*
*/

#define TROWBRIDGE_REITZ_GGX
#define SMITH_SCHLICK_GEOMETRY
#define FRESNEL_SCHLICK
#define COOK_TORRANCE_BRDF

#ifndef PI
#define PI 3.14159265f
#endif

// Clamps the dot product to 0.0f if it is negative
float ClampedDot(float3 vector0, float3 vector1)
{
    return max(dot(vector0, vector1), 0.0f);
}

float DistributionFunction(float3 normal, float3 halfway, float roughness)
{
#ifdef TROWBRIDGE_REITZ_GGX
    /*
    * Trowbridge-Reitz GGX:
    * D(n, h, a) = (a ^ 2) / {PI * [((n * h) ^ 2)(a ^ 2 - 1) + 1] ^ 2} ; h is the halfway vector to measure against the surface's microfacets
    */
    roughness = roughness * roughness;
    float power_two_roughness = roughness * roughness;
    float n_dot_h = ClampedDot(normal, halfway);
    float n_dot_h_squared = n_dot_h * n_dot_h;
    
    float numerator = power_two_roughness;
    float denominator = n_dot_h_squared * (power_two_roughness - 1) + 1;
    denominator = PI * denominator * denominator;
    
    return numerator / denominator;
#endif
}

float GeometrySchlick(float3 normal, float3 view, float k)
{
    /*
    * Gs(n, v, k) = (n * v) / ((n * v)(1 - k) + k) 
    */
    
    // Clamp the dot to 0.0f if it is negative
    float n_dot_v = ClampedDot(normal, view);
    float denominator = n_dot_v * (1 - k) + k;
    return n_dot_v / denominator;
}

float GeometryFunction(float3 normal, float3 view, float3 light_direction, float k)
{
#ifdef SMITH_SCHLICK_GEOMETRY
    // G(n, v, l, k) = Gs(n, v, k) * Gs(n, l, k)
    
    return GeometrySchlick(normal, view, k) * GeometrySchlick(normal, light_direction, k);
    
#endif
}

float GeometryFactorDirect(float roughness)
{
    // k = ((a + 1) ^ 2) / 8
    return (roughness + 1) * (roughness + 1) / 8;
}

float GeometryFactorIBL(float roughness)
{
    // k = (a ^ 2) / 2
    return roughness * roughness / 2;
}

// F0 factor in fresnel approximation
float3 FresnelBaseReflectivity(float3 surface_color, float metallic)
{
    return lerp(float3(0.04f, 0.04f, 0.04f), surface_color, metallic);
}

float3 FresnelSchlick(float cos_theta, float3 F0)
{
    // F(h, v, F0) = F0 + (1 - F0)[(1 - (h * v)) ^ 5]
    // Clamp the inverse to prevent black spots
    return F0 + (1 - F0) * pow(clamp(1 - cos_theta, 0.0f, 1.0f), 5);
}

float3 FresnelSchlickRoughness(float cos_theta, float3 F0, float roughness)
{
    // F(h, v, F0) = F0 + (1 - F0)[(1 - (h * v)) ^ 5]
    // Clamp the inverse to prevent black spots
    float roughness_factor = 1.0f - roughness;
    return F0 + (max(float3(roughness_factor, roughness_factor, roughness_factor), F0) - F0) * pow(clamp(1 - cos_theta, 0.0f, 1.0f), 5);
}

float3 FresnelEquation(float cos_theta, float3 F0)
{
#ifdef FRESNEL_SCHLICK
    return FresnelSchlick(cos_theta, F0);
#endif
}

// Cook-Torrance BRDF
float3 CalculateBRDF(float3 radiance, float3 surface_color, float3 normal, float3 view_direction, float3 light_direction, float3 halfway, float3 F0, float metallic, float roughness)
{
#ifdef COOK_TORRANCE_BRDF
    float geometry_k_factor = GeometryFactorDirect(roughness);
    
    float3 fresnel_factor = FresnelEquation(ClampedDot(halfway, view_direction), F0);
    float normal_distribution_factor = DistributionFunction(normal, halfway, roughness);
    float geometry_factor = GeometryFunction(normal, view_direction, light_direction, geometry_k_factor);
    
    float3 numerator = fresnel_factor * normal_distribution_factor * geometry_factor;
    float denominator = 4.0f * ClampedDot(normal, view_direction) * ClampedDot(normal, light_direction) + 0.0001f;
    float3 specular = numerator / denominator;
    
    float3 kS = fresnel_factor;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    kD *= 1.0f - metallic;
    
    float n_dot_l = ClampedDot(normal, light_direction);
    return (kD * surface_color / PI + specular) * radiance * n_dot_l;
#endif
}

float PointLightAttenuation(float distance, float light_range)
{
    // Formula: [saturate(1 - (distance / light_range) ^ 4) ^ 2] / (distance ^ 2 + 1)
    float numerator = saturate(1 - pow(distance / light_range, 4));
    numerator = numerator * numerator;
    float denominator = distance * distance + 1;
    return numerator / denominator;
}

float3 CalculatePointLight(
    float3 light_position,
    float3 light_color,
    float light_range,
    float3 world_position,
    float3 surface_color,
    float3 normal,
    float3 view_direction,
    float3 F0,
    float metallic,
    float roughness
)
{
    float3 light_direction = normalize(light_position - world_position);
    float3 halfway = normalize(light_direction + view_direction);
        
    float distance = length(light_position - world_position);
    float attenuation = PointLightAttenuation(distance, light_range);
        
    float3 radiance = light_color * attenuation;
        
    return CalculateBRDF(radiance, surface_color, normal, view_direction, light_direction, halfway, F0, metallic, roughness);
}

float3 CalculateDirectionalLight(
    float3 light_direction,
    float3 view_direction,
    float3 radiance,
    float3 surface_color,
    float3 normal,
    float3 F0,
    float metallic,
    float roughness
)
{
    light_direction = normalize(light_direction);
    float3 halfway = normalize(light_direction + view_direction);
    return CalculateBRDF(radiance, surface_color, normal, view_direction, light_direction, halfway, F0, metallic, roughness);
}

float3 CalculateAmbient(
    float3 view_direction, 
    float3 normal, 
    float3 surface_color,
    float3 F0, 
    float roughness,
    float metallic,
    TextureCube<float4> environment_diffuse,
    TextureCube<float4> environment_specular, 
    Texture2D<float2> brdf_lut, 
    SamplerState environment_sampler,
    float environment_specular_max_mip,
    float diffuse_factor,
    float specular_factor
)
{
    float v_dot_n = ClampedDot(view_direction, normal);
    float3 specular_F = FresnelSchlickRoughness(v_dot_n, F0, roughness);
    
    // Diffuse IBL
    float3 ambient_diffuse = environment_diffuse.Sample(environment_sampler, normal).rgb;
   
    // Specular IBL
    float3 reflection_direction = reflect(-view_direction, normal);
    float3 prefiltered_ambient_specular = environment_specular.SampleLevel(environment_sampler, reflection_direction, roughness * environment_specular_max_mip).rgb;
    
    float2 ambient_brdf = brdf_lut.Sample(environment_sampler, float2(v_dot_n, roughness)).rg;
    float3 ambient_specular = prefiltered_ambient_specular * (specular_F * ambient_brdf.x + ambient_brdf.y);
    
    ambient_diffuse *= surface_color * (1.0f - specular_F) * (1.0f - metallic);
    
    return ambient_diffuse * diffuse_factor + ambient_specular * specular_factor;
}

// Returns a sample inside the specular lobe of the reflected light according to the roughness parameter
// It will be biased using importance sampling
// Xi is a low discrepancy sequence random number
float3 ImportanceSampleGGX(float2 xi, float3 normal, float roughness)
{
    float a = roughness * roughness;
    
    float phi = 2 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    
    // Transformation from spherical coordinates to cartesian coordinates using r as 1.0f
    float3 cartesian_coordinates = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
    
    // Transform from tangent space into world space
    float3 up = abs(normal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = normalize(cross(normal, tangent));
    
    float3 sample = cartesian_coordinates.x * tangent + cartesian_coordinates.y * bitangent + cartesian_coordinates.z * normal;
    return normalize(sample);
}

float2 IntegrateBRDF(float n_dot_v, float roughness, uint sample_count)
{
    float3 view_direction;
    // Sin theta
    view_direction.x = sqrt(1.0f - n_dot_v * n_dot_v);
    view_direction.y = 0.0f;
    // Cos theta
    view_direction.z = n_dot_v;

    float x = 0.0f;
    float y = 0.0f;

    float3 normal = float3(0.0f, 0.0f, 1.0f);
    
    for (uint index = 0; index < sample_count; index++)
    {
        float2 xi = Hammersley(index, sample_count);
        float3 halfway = ImportanceSampleGGX(xi, normal, roughness);
        float3 light_direction = normalize(-reflect(halfway, normal));

        float n_dot_l = ClampedDot(light_direction, normal);
        float n_dot_h = ClampedDot(halfway, normal);
        float v_dot_h = ClampedDot(view_direction, halfway);

        if (n_dot_l > 0.0f)
        {
            float geometry = GeometryFunction(normal, view_direction, light_direction, GeometryFactorIBL(roughness));
            float geometry_factored = (geometry * v_dot_h) / (n_dot_h * n_dot_v);
            float fresnel_factor = pow(1.0f - v_dot_h, 5.0f);

            x += (1.0f - fresnel_factor) * geometry_factored;
            y += fresnel_factor * geometry_factored;
        }
    }
    x /= float(sample_count);
    y /= float(sample_count);
    return float2(x, y);
}

#endif