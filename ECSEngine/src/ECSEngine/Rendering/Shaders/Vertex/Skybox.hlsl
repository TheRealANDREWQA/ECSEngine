cbuffer Matrices
{
    float4x4 view_projection;
}

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float3 position : POSITION;
};

VS_OUTPUT main(in VS_INPUT input, out float4 clip_position : SV_Position)
{
    VS_OUTPUT output;
    clip_position = mul(float4(input.position, 1.0f), view_projection);
    clip_position = clip_position.xyww;
    output.position = input.position;
    return output;
}