#include "../Macros.hlsli"

struct VS_INPUT
{
    float3 position : POSITION;
    // This is the instance index of the draw call
    uint instance_index : INSTANCE_INDEX;
};

struct VS_OUTPUT
{
    uint instance_index_thickness : INSTANCE_INDEX_THICKNESS;
};

struct InstancedMatrixData
{
    float4x4 transformation;
};

struct InstancedIDData
{
    uint instance_index_thickness;
};

StructuredBuffer<InstancedMatrixData> instanced_matrix_data : register(t0);
StructuredBuffer<InstancedIDData> instanced_id_data : register(t1);

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
       
    position = mul(float4(input.position, 1.0f), instanced_matrix_data[input.instance_index].transformation);
    output.instance_index_thickness = instanced_id_data[input.instance_index].instance_index_thickness;
    
    return output;
}