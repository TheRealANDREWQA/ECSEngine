cbuffer Matrices
{
    float4x4 projection_view_matrix;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float3 position : POSITION;
};

VS_OUTPUT main(in VS_INPUT input, out float4 position : SV_Position)
{
    VS_OUTPUT output;
    
    output.position = input.position;
    position = mul(float4(input.position, 1.0f), projection_view_matrix);
    
    return output;
}