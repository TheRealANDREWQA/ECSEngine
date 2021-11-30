#include "../Utilities.hlsli"

RWTexture2D<unorm float4> render_target : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    float4 value = render_target[dispatch_id.xy];
    value = float4(LinearToSRGB(value.rgb), value.a);
    render_target[dispatch_id.xy] = value;
}