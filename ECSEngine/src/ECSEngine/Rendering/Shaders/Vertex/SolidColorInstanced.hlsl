#include "../Macros.hlsli"

// Color will be a color float since it must be aligned to 16 byte boundaries
struct VS_INPUT
{
    float3 position : POSITION;
    float4x4 world_matrix : WORLD_MATRIX; ECS_REFLECT_INSTANCE(1) ECS_REFLECT_INPUT_SLOT(1) 
    float4 color : COLOR; ECS_REFLECT_INSTANCE(1) ECS_REFLECT_INPUT_SLOT(1) 
};

struct VS_OUTPUT
{
    float4 color : COLOR;
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;

    position = mul(float4(input.position, 1.0f), input.world_matrix);
    output.color = input.color;
    
    return output;
}