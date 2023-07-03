Texture2D<uint> depth_texture : register(t0);
RWTexture2D<unorm float4> render_target : register(u0);

cbuffer Thickness
{
    float4 border_color;
    uint pixel_thickness;
};

[numthreads(16, 16, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint stencil = depth_texture[dispatch_id.xy];
    
    // Get the stencil value
    if (stencil == 0)
    {
        // This is outside the render bounds - now check neighbours
        for (int x = -pixel_thickness; x <= pixel_thickness; x++)
        {
            for (int y = -pixel_thickness; y <= pixel_thickness; y++)
            {
                uint neighbour_stencil = depth_texture[dispatch_id.xy];
                if (neighbour_stencil != 0)
                {
                    render_target[dispatch_id.xy] = border_color;
                    return;
                }
            }
        }
    }
}