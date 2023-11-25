#include "../Macros.hlsli"

// Uses a cbuffer to indicate the color
cbuffer Color
{
    float4 color; ECS_REFLECT_AS_COLOR
};

float4 main() : SV_TARGET
{
    return color;
}