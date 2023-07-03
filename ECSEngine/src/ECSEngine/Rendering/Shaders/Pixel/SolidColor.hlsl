// Uses a cbuffer to indicate the color
cbuffer Color
{
    float4 color;
};

float4 main() : SV_TARGET
{
    return color;
}