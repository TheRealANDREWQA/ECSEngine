#include "../Utilities.hlsli"

#define COLORIZE_SIZE 256
#define COLORIZE_MODULO_AND 255

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
    uint perform_srgb;
    // If this is true, then it will use the colorize constant buffer
    // There needs to be 1024 colors into the colorize buffer
    uint colorize;
}

cbuffer Colorize : register(b1)
{
    float4 colorize_values[COLORIZE_SIZE];
}


#ifdef FLOAT
Texture2D<unorm float4> input_values : register(t0);
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
    uint perform_srgb,
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
    
    value = value * normalize_factor + offset;
    
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
    
    if (perform_srgb)
    {
        value.xyz = LinearToSRGB(value.xyz);
    }
    
    return value;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    float4 input = float4(0.0f, 0.0f, 0.0f, 0.0f);
    uint local_keep_channel = keep_channel;
    uint local_channel_count = channel_count;
    float local_offset = offset;
    float local_normalize_factor = normalize_factor;
    
#if defined(DEPTH) || defined(FLOAT)
    input = input_values[dispatch_id.xy];
#elif defined(SINT) || defined(UINT)
    // Check to see if colorization is enabled
    if (colorize) {
        // Treat both SINT and UINT the same, just hash the value into the colorize array
        uint4 input_uint = (uint4)input_values[dispatch_id.xy];
        // For the colorization, use this simple rule to get the hash value to use in the
        // colorize_values - add all channels together. This also is nice since channels that
        // got extended won't affect the other channels
        uint colorize_hash = input_uint.x + input_uint.y + input_uint.z + input_uint.w;
        input = colorize_values[colorize_hash & COLORIZE_MODULO_AND];
    
        // We can also disable the offset and the normalization just to be safe
        local_offset = float4(0.0f, 0.0f, 0.0f, 0.0f);
        local_normalize_factor = float4(1.0f, 1.0f, 1.0f, 1.0f);
        local_keep_channel = 1;
    }
    else {
        input = input_values[dispatch_id.xy];
    }
#endif
    
#ifdef DEPTH
    local_keep_channel = 0;
    local_channel_count = 1;
    // For depth we must also perform the transformation
    // Approximate the value with a simple formula
    input = float4(0.001f, 0.001f, 0.001f, 0.001f) / (float4(1.001f, 1.001f, 1.001f, 1.001f) - input);
#endif
    
    output_values[dispatch_id.xy] = VisualizeValue(
        input, 
        keep_red, 
        keep_green, 
        keep_blue, 
        keep_alpha, 
        local_keep_channel, 
        local_channel_count, 
        perform_srgb, 
        local_offset, 
        local_normalize_factor
    );
}