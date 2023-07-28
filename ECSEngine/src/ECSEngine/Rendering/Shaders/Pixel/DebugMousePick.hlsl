// The output value is already an instance_index coalesced with pixel thickness
struct PS_INPUT
{
    uint instance_index_thickness : INSTANCE_INDEX_THICKNESS;
};

uint main(in PS_INPUT input) : SV_TARGET
{
    return input.instance_index_thickness;
}