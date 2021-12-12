#ifndef __UTILITIES__
#define __UTILITIES__

#ifndef PI
#define PI 3.14159265f
#endif

float3 GetClosestPointToSegment(float3 original_point, float3 segment_point, float3 segment_normalized_direction, float segment_length)
{
    // Calculate the closest point from the line segment to the point
    float3 point_to_segment = original_point - segment_point;
    
    // The percentage of the line segment size
    float line_segment_size = dot(point_to_segment, segment_normalized_direction) / segment_length;
    
    // Saturate the segment size and multiply again with the light direction length
    return segment_point + segment_normalized_direction * (saturate(line_segment_size) * segment_length);
}

// Christian Schuler, "Normal Mapping without Precomputed Tangents", ShaderX 5
// Follow-up blog: http://www.thetenthplanet.de/archives/1180/comment-page-1
// Returns the cotangent frame computed from the partial derivates of the uv texture
float3x3 CotangentFrame(float3 world_space_normal, float3 world_position, float2 uv)
{
    float3 position_derivative_x = ddx(world_position);
    float3 position_derivative_y = ddy(world_position);
    float2 uv_derivative_x = ddx(uv);
    float2 uv_derivative_y = ddy(uv);
    
    float3 covector_x = cross( position_derivative_y, world_space_normal ); 
    float3 covector_y = cross( world_space_normal, position_derivative_x ); 
    
    float3 tangent = covector_x * uv_derivative_x.x + covector_y * uv_derivative_y.x;
    float3 bitangent = covector_x * uv_derivative_x.y + covector_y * uv_derivative_y.y;
    
    float maximum_inverse_root = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    return float3x3(tangent * maximum_inverse_root, bitangent * maximum_inverse_root, world_space_normal);
}

// Christian Schuler, "Normal Mapping without Precomputed Tangents", ShaderX 5
// Follow-up blog: http://www.thetenthplanet.de/archives/1180/comment-page-1
// Returns the value of the normal transformed by the TBN space
float3 ComputePixelNormal(float3 local_normal, float3 world_position, float3 world_space_normal, float2 uv, float strength)
{
    float3 unpacked_normal = local_normal * 2.0f - 1;  
    unpacked_normal.xy *= float2(strength, strength);
    unpacked_normal.z = sqrt(1.0f - dot(unpacked_normal.xy, unpacked_normal.xy));

    float3x3 TBN = CotangentFrame(world_space_normal, world_position, uv);
    
    return normalize(mul(unpacked_normal, TBN));
}

float3 LinearToSRGB(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float3 SRGBToLinear(float3 color)
{
    return pow(color, 2.2f);
}

// Direction must be normalized
float2 SampleSphericalMap(float3 direction)
{
    static const float2 atan_inverse = float2(0.1591f, 0.3183f);
    float2 uv = float2(atan2(direction.z, direction.x), asin(direction.y));
    uv *= atan_inverse;
    return uv + float2(0.5f, 0.5f);
}

// Converts the coordinates according to this formula:
// x = r sin(theta) * cos(phi)
// y = r sin(theta) * sin(phi)
// z = r cos(theta)
// r = the length of the coordinates
// theta - the zenith angle similar to latitude
// phi - the azimuth angle similar to longitude
// Returns the triple (r, theta, phi)
float3 CartesianToSphericalCoordinates(float3 coordinates)
{
    float r = length(coordinates);
    float r_reciprocal = rcp(r);
    float cos_theta = coordinates.z * r_reciprocal;
    float sin_theta_sin_phi = coordinates.y * r_reciprocal;
    float sin_theta_cos_phi = coordinates.x * r_reciprocal;
    
    float theta = acos(cos_theta);
    // Sin theta could be calculated as sqrt from 1 - cos(theta) ^ 2 but theta must be deduced, because it is one of the variables
    float sin_theta = sin(theta);
    float r_sin_theta = rcp(sin_theta);
    float cos_phi = sin_theta_cos_phi * r_sin_theta;
    float sin_phi = sin_theta_sin_phi * r_sin_theta;
    float phi = atan2(cos_phi, sin_phi);
    
    return float3(r, theta, phi);
}

// The coordinates must be expressed as (r, theta, phi)
// Converts the coordinates according to this formula:
// x = r sin(theta) * cos(phi)
// y = r sin(theta) * sin(phi)
// z = r cos(theta)
// r = the length of the coordinates
// theta - the zenith angle similar to latitude
// phi - the azimuth angle similar to longitude
float3 SphericalCoordinatesToCartesian(float r, float theta, float phi)
{
    float sin_theta = sin(theta);
    float cos_theta = cos(theta);
    float sin_phi = sin(phi);
    float cos_phi = cos(phi);
    
    return float3(r * sin_theta * cos_phi, r * sin_theta * sin_phi, r * cos_theta);
}

float RadicalInverseVanDerCorput(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // the same as dividing by 0x100000000
}

float2 Hammersley(uint index, uint sample_count)
{
    return float2(float(index) / float(sample_count), RadicalInverseVanDerCorput(index));
}

float3 TonemapReinhard(float3 irradiance)
{
    return irradiance / (irradiance + 1.0f);
}

// Tipical values for saturation_point : [4; 50]
float3 TonemapReinhardModified(float3 irradiance, float3 saturation_point)
{
    return (irradiance * (float3(1.0f, 1.0f, 1.0f) + irradiance / saturation_point)) / (irradiance + 1);
}

// Credit to https://www.shadertoy.com/view/WdjSW3 and https://bruop.github.io/tonemapping/
float3 TonemapACES(float3 irradiance)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    // Formula: (x * (a * x + b)) / (x * (c * x + d) + e); where x is irradiance
    // and a, b, c, d, e are constants
    return (irradiance * mad(irradiance, a, b)) / (irradiance * mad(irradiance, c, d) + e);
}

// Credit to https://www.shadertoy.com/view/WdjSW3 and https://bruop.github.io/tonemapping/
// Already has gamma correction applied
float3 TonemapACESApproximation(float3 irradiance)
{
    return irradiance / (irradiance + 0.155f) * 1.019f;
}

// Credit to https://www.shadertoy.com/view/WdjSW3 and https://bruop.github.io/tonemapping/
float3 TonemapUchimura(float3 irradiance, float3 P, float3 a, float3 m, float3 l, float3 c, float3 b)
{
    float3 l0 = ((P - m) * l) / a;
    float3 L0 = m - m / a;
    float3 L1 = m + (1.0 - m) / a;
    float3 S0 = m + l0;
    float3 S1 = m + a * l0;
    float3 C2 = (a * P) / (P - S1);
    float3 CP = -C2 / P;

    float3 w0 = 1.0 - smoothstep(0.0, m, irradiance);
    float3 w2 = step(m + l0, irradiance);
    float3 w1 = 1.0 - w0 - w2;

    float3 T = m * pow(irradiance / m, c) + b;
    float3 S = P - (P - S1) * exp(CP * (irradiance - S0));
    float3 L = m + a * (irradiance - m);

    return T * w0 + L * w1 + S * w2;
}

// Credit to https://www.shadertoy.com/view/WdjSW3 and https://bruop.github.io/tonemapping/
float3 TonemapUchimura(float3 irradiance)
{
    const float P = 1.0;
    const float a = 1.0;
    const float m = 0.22;
    const float l = 0.4;
    const float c = 1.33;
    const float b = 0.0;
    return TonemapUchimura(irradiance, P, a, m, l, c, b);
}

// Credit to https://www.shadertoy.com/view/WdjSW3 and https://bruop.github.io/tonemapping/
float3 TonemapLottes(float3 irradiance)
{
    // Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
    const float a = 1.6f;
    const float d = 0.977f;
    const float hdrMax = 8.0f;
    const float midIn = 0.18f;
    const float midOut = 0.267f;

    static const float b =
        (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
        ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    static const float c =
        (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
        ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(irradiance, a) / (pow(irradiance, a * d) * b + c);
}

#endif