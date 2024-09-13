Texture2D font_texture;

SamplerState font_sampler : register(s0);

struct VSOutput {
	float4 position : SV_POSITION;
	float4 color : Color;
	float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
	float4 texture_color = font_texture.Sample(font_sampler, input.uv) * input.color;
    //if (texture_color.r >= 0.35f && texture_color.g >= 0.35f && texture_color.b >= 0.35f)
    //    return float4(texture_color.rgb, 1.0f);
    //return float4(texture_color.rgb, 0.0f);
    texture_color.a = saturate(texture_color.a * 1.0f);
    return texture_color;
}