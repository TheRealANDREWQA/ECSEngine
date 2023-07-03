// Uses a cbuffer to indicate the value
cbuffer Color
{
    uint value;
};

uint main() : SV_TARGET
{
    return value;
}