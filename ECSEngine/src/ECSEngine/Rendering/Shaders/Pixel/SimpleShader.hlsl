cbuffer Colors : register(b0)
{
    float4 vertex_color[6];
};

float4 main(uint triangle_index : SV_PrimitiveID) : SV_Target0
{
    return vertex_color[triangle_index % 6];
}