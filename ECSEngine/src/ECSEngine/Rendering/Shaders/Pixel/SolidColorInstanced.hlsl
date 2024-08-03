struct PS_INPUT
{
    float4 color : COLOR;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
    return input.color;
}