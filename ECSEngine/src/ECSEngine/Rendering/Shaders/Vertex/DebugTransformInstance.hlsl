#include "../Macros.hlsli"

// Color will be a color float since it must be aligned to 16 byte boundaries
struct VS_INPUT
{
    float3 position : POSITION;
    float4x4 world_matrix : WORLD_MATRIX; ECS_REFLECT_INSTANCE(1) ECS_REFLECT_INPUT_SLOT(1) 
    uint instance_index_thickness : COLOR; ECS_REFLECT_INSTANCE(1) ECS_REFLECT_INPUT_SLOT(2) 
};

struct VS_OUTPUT
{
    uint instance_index_thickness : INSTANCE_INDEX_THICKNESS;
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;

    position = mul(float4(input.position, 1.0f), input.world_matrix);
    output.instance_index_thickness = input.instance_index_thickness;
    
    return output;
}