Texture2D sprite_texture;

SamplerState sprite_sampler : register(s0);

struct VSOutput {
	float4 position : SV_POSITION;
	float4 color : Color;
	float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
	return sprite_texture.Sample(sprite_sampler, input.uv) * input.color;
}