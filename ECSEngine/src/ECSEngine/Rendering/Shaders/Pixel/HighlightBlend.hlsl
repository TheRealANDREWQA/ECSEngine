// Uses a cbuffer to indicate the value
cbuffer Parameters : register(b0)
{
    float4 color;
    int pixel_count;
};

struct PS_INPUT
{
    float2 uv : UV;
};

Texture2D<uint> stencil_texture : register(t0);

float4 main(in PS_INPUT input) : SV_TARGET
{
    uint width, height;
    stencil_texture.GetDimensions(width, height);
    int2 texel_indices = int2(float2(width, height) * input.uv);
    uint stencil_value = stencil_texture.Load(int3(texel_indices, 0));
    if (stencil_value == 0)
    {
        // Check to see if it falls in the range of a stencil pixel
        [loop]
        for (int x = -pixel_count; x <= pixel_count; x++)
        {
            [loop]
            for (int y = -pixel_count; y <= pixel_count; y++)
            {
                uint neighbour_stencil = stencil_texture.Load(int3(texel_indices + int2(x, y), 0));
                if (neighbour_stencil != 0)
                {
                    return float4(color.xyz, 1.0f);
                }
            }
        }

    }
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}