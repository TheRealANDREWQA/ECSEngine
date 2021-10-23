// Useful constant buffers and structures

/*
cbuffer HemisphericConstants
{
    float3 AmbientDown : packoffset(c0);
    float3 AmbientUp : packoffset(c1);
}

cbuffer SpecularFactors
{
    float SpecularExponent : packoffset(c0.x);
    float SpecularIntensity : packoffset(c0.y);
}

cbuffer CameraPosition
{
    float3 CameraPosition;
}

struct DirectionalLight
{
    // Light Direction has to be normalized
    float3 direction : packoffset(c0);
    float3 color : packoffset(c1);
}

cbuffer DirectionalLightBuffer 
{
    // Light Direction has to be normalized
    float3 DirectionalLightDirection : packoffset(c0);
    float3 DirectionalLightColor : packoffset(c1);
}

struct PointLight
{
    float3 position : packoffset(c0);
    float range_reciprocal : packoffset(c0.w);
    float3 color : packoffset(c1);
    float attenuation_power : packoffset(c1.w);
}

cbuffer PointLightBuffer
{
    float3 PointLightPosition : packoffset(c0);
    float PointLightRangeReciprocal : packoffset(c0.w);
    float3 PointLightColor : packoffset(c1);
    float PointLightAttenuationPower : packoffset(c1.w);
}

struct SpotLight
{
    // Light Direction has to be normalized
    float3 position : packoffset(c0);
    float range_reciprocal : packoffset(c0.w);
    float3 direction : packoffset(c1);
    float cosine_outer_cone : packoffset(c1.w);
    float3 color : packoffset(c2);
    // This is the difference between the inner cosine and the outer cosine
    float cosine_inner_cone_reciprocal : packoffset(c2.w);
    float cone_attenuation : packoffset(c3);
    float range_attenuation : packoffset(c3.y);
}

cbuffer SpotLightBuffer
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

*/

float3 CalculateAmbient(float3 normal, float3 color, float3 ambient_down_color, float3 ambient_up_color)
{
    // Convert the normal from [-1, 1] to [0, 1]
    float normal_up = mad(normal.y, 0.5f, 0.5f);
    
    // Calculate the ambient value
    float3 ambient_value = mad(ambient_up_color, float3(normal_up, normal_up, normal_up), ambient_down_color);
    
    // Apply the ambient value to the color
    return ambient_value * color;
}

float3 CalculatePhongDiffuse(float3 normalized_normal, float3 normalized_direction_to_light, float3 light_color)
{
    // Diffuse lighting is proportional to the dot product of the normal and the direction to the light
    float3 normal_to_light_dot = dot(normalized_normal, normalized_direction_to_light);
    return light_color * saturate(normal_to_light_dot);
}

float3 CalculateBlinnSpecular(
    float3 normalized_normal,
    float3 normalized_view_direction,
    float3 normalized_light_direction, 
    float specular_exponent,
    float specular_intensity, 
    float3 directional_light_color
)
{
    // H - half way - is the half way vector between the view direction and the light direction
    // The specular value is proportional to the dot product of the H vector and the N vector
    // The specular factors affect how large and how bright the specular component will be:
    // Higher specular_exponent -> smaller specular reflection
    // Higher specular_intensity -> brighter specular reflection
    float3 half_way = normalize(normalized_view_direction + normalized_light_direction);
    float normal_to_light_dot = saturate(dot(half_way, normalized_normal));
    return directional_light_color * pow(normal_to_light_dot, specular_exponent) * specular_intensity;
}

float3 CalculateBlinnPhong(
    float3 normalized_normal,
    float3 normalized_view_direction,
    float3 normalized_light_direction,
    float specular_exponent,
    float specular_intensity,
    float3 light_color,
    float3 pixel_color
)
{
    // The sum of the diffuse component and the specular component
    float3 diffuse = CalculatePhongDiffuse(normalized_normal, normalized_light_direction, light_color);
    float3 specular = CalculateBlinnSpecular(
        normalized_normal,
        normalized_view_direction, 
        normalized_light_direction,
        specular_exponent,
        specular_intensity, 
        light_color
    );
    return (diffuse + specular) * pixel_color;
}

float CalculateLightRangeAttenuation(float distance_to_light, float light_range_reciprocal, float light_attenuation_exponent)
{
    float distance_to_light_factor = 1.0f - saturate(distance_to_light * light_range_reciprocal);
    return pow(distance_to_light_factor, light_attenuation_exponent);
}

float3 CalculatePointLight(
    float3 normalized_normal,
    float3 normalized_view_direction,
    float3 world_position,
    float3 pixel_color,
    float3 light_position,
    float3 light_color,
    float light_range_reciprocal,
    float light_attenuation,
    float specular_exponent,
    float specular_intensity
)
{
    // Calculate the direction and the direction to the light
    float3 light_direction = light_position - world_position;
    float distance_to_light = length(light_direction);
    
    float3 normalized_light_direction = normalize(light_direction);
    
    // Use the normal blinn phong shader equation
    float3 blinn_phong = CalculateBlinnPhong(
        normalized_normal,
        normalized_view_direction,
        normalized_light_direction,
        specular_exponent,
        specular_intensity,
        light_color,
        pixel_color
    );

    // Attenuation
    float attenuation = CalculateLightRangeAttenuation(distance_to_light, light_range_reciprocal, light_attenuation);
    return blinn_phong * attenuation;
}

float3 CalculateSpotLight(
    float3 normalized_normal,
    float3 normalized_view_direction,
    float3 world_position,
    float3 pixel_color,
    float3 light_position,
    float3 light_direction,
    float3 light_color,
    float light_range_reciprocal,
    float light_cosine_outer,
    float light_cosine_inner_reciprocal,
    float light_cone_attenuation,
    float light_range_attenuation,
    float specular_exponent,
    float specular_intensity
)
{
    // Calculate the direction and the distance from the pixel to the light
    float3 point_to_light_direction = light_position - world_position;
    float distance_to_light = length(point_to_light_direction);
    
    float3 normalized_point_to_light_direction = normalize(point_to_light_direction);
    
    // Use the blinn phong equation for shading
    float3 blinn_phong = CalculateBlinnPhong(
        normalized_normal,
        normalized_view_direction,
        normalized_point_to_light_direction,
        specular_exponent,
        specular_intensity,
        light_color,
        pixel_color
    );
    
    float cos_angle = dot(normalized_point_to_light_direction, light_direction);
    
    // Cone attenuation
    float cone_attenuation = saturate((cos_angle - light_cosine_outer) * light_cosine_inner_reciprocal);
    cone_attenuation = pow(cone_attenuation, light_cone_attenuation);
    
    // Attenuation
    float range_attenuation = CalculateLightRangeAttenuation(distance_to_light, light_range_reciprocal, light_range_attenuation);
    return blinn_phong * cone_attenuation * range_attenuation;
}