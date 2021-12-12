#include "../Macros.hlsli"

// Color will be a color float since it must be aligned to 16 byte boundaries
cbuffer MeshData
{
    float4x4 MVP_matrix;
    float4 color;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 color : COLOR;
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;

    position = mul(float4(input.position, 1.0f), MVP_matrix);
    output.color = color;
    
    return output;
}