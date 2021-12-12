#include "../Macros.hlsli"

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 color : COLOR;
};

struct InstancedData
{
    float4x4 transformation;
    float4 color;
};

StructuredBuffer<InstancedData> instanced_data;

VS_OUTPUT main( in VS_INPUT input, in uint instanced_index : SV_InstanceID, out float4 position : SV_Position )
{
    VS_OUTPUT output;
       
    position = mul(float4(input.position, 1.0f), instanced_data[instanced_index].transformation);
    output.color = instanced_data[instanced_index].color;
    
    return output;
}