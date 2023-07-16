cbuffer Conversion : register(b0)
{
    float4 offset;
    float4 normalize_factor;
    uint keep_red;
    uint keep_green;
    uint keep_blue;
    uint keep_alpha;
    uint keep_channel;
    uint channel_count;
}

#ifdef FLOAT
Texture2D<float4> input_values : register(t0);
#else
#ifdef UINT
Texture2D<uint4> input_values : register(t0);
#else
#ifdef SINT
Texture2D<int4> input_values : register(t0);
#else
#ifdef DEPTH
Texture2D<float> input_values : register(t0);
#endif
#endif
#endif
#endif
RWTexture2D<float4> output_values : register(u0);

float4 VisualizeValue(
    float4 value,
    uint keep_red,
    uint keep_green,
    uint keep_blue,
    uint keep_alpha,
    uint keep_channel,
    uint channel_count,
    float4 offset,
    float4 normalize_factor
)
{
    if (!keep_channel)
    {
        if (channel_count == 1)
        {
            value = value.rrra;
        }
    }
    
    if (channel_count != 4)
    {
        value.a = 1.0f;
    }
    
    if (!keep_red)
    {
        value.r = 0.0f;
    }
    
    if (!keep_green)
    {
        value.g = 0.0f;
    }
    
    if (!keep_blue)
    {
        value.b = 0.0f;
    }
    
    if (!keep_alpha)
    {
        value.a = 1.0f;
    }
    
    return value * normalize_factor + offset;
}


[numthreads(16, 16, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
#if defined(DEPTH) || defined(SINT) || defined(UINT) || defined(FLOAT)
    float4 input = input_values[dispatch_id.xy];
#else
    float4 input = float4(0.0f, 0.0f, 0.0f, 0.0f);
#endif
    
    uint local_keep_channel = keep_channel;
    uint local_channel_count = channel_count;
#ifdef DEPTH
    local_keep_channel = 0;
    local_channel_count = 1;
    // For depth we must also perform the transformation
    // Approximate the value with a simple formula
    input = float4(0.001f, 0.001f, 0.001f, 0.001f) / (float4(1.001f, 1.001f, 1.001f, 1.001f) - input);
#endif
    
    output_values[dispatch_id.xy] = VisualizeValue(input, keep_red, keep_green, keep_blue, keep_alpha, local_keep_channel, local_channel_count, offset, normalize_factor);
}