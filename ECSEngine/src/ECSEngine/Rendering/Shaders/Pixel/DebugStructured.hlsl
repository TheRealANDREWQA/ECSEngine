struct PS_INPUT
{
    float3 color : COLOR;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}