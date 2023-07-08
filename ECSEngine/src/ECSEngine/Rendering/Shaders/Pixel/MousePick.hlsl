// This shader just outputs the resulting value from the vertex shader
// The output value is already an instance_index coalesced with pixel thickness
struct PS_INPUT
{
    
};

cbuffer InstanceIndexThickness
{
    uint value;
};

uint main(in PS_INPUT input) : SV_TARGET
{
    return value;
}