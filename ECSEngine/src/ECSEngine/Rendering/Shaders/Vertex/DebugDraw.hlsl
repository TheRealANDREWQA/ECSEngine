#include "..\Macros.hlsli"

struct VS_INPUT ECS_REFLECT_INCREMENT_INPUT_SLOT
{
    float3 position : POSITION;
    float4x4 world_matrix : WORLD_MATRIX; ECS_REFLECT_INSTANCE(1)  
    float4 color : COLOR; ECS_REFLECT_UNORM_8 ECS_REFLECT_INSTANCE(1)
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

VS_OUTPUT main(in VS_INPUT input)
{
    VS_OUTPUT output;

    output.position = mul(float4(input.position, 1.0f), input.world_matrix);
    output.color = input.color;
    
	return output;
}