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

#endif