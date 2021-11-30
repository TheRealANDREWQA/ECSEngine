#include "../Utilities.hlsli"

struct PS_INPUT
{
    float3 position : POSITION;
};

TextureCube environment_map;
SamplerState map_sampler;

cbuffer SampleDelta
{
    float DeltaStep;
};

// Integration of the PBR diffuse part using discrete samples at fixed intervals
// Equivalent to a Riemann sum
// The coordinates are transformed into spherical coordinates for easier computation
// The polar azimuth phi angle samples around the hemisphere from 0 to 2PI like longitude
// The zenith theta angle samples around the hemisphere from 0 to PI/2 like latitude
float4 main(in PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.position);
    
    float3 irradiance = 0.0f;
    
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));
    
    uint sample_count = 0;
    for (float phi = 0; phi < 2 * PI; phi += DeltaStep)
    {
        for (float theta = 0; theta < PI / 2; theta += DeltaStep)
        {
            // tangent space sample
            float3 tangent_sample = SphericalCoordinatesToCartesian(1.0f, theta, phi);
            
            // Convert to world space sample
            float3 world_space_sample = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal;
            // The y coordinate must be flipped - otherwise the cube map will be flipped on the y axis
            world_space_sample.y = -world_space_sample.y;
            
            // * cos(theta) from n * wi (the dot product of the normal and the incoming light ray)
            // * cos(theta) to reduce the contribution of samples located at stepper angles because they cover
            // a smaller solid angle
            irradiance += environment_map.Sample(map_sampler, world_space_sample) * cos(theta) * cos(theta);
            sample_count++;
        }

    }
    irradiance *= PI / (float)sample_count;
    
    return float4(irradiance, 1.0f);
}