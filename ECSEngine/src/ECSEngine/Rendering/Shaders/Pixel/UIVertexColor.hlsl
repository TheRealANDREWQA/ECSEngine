struct VSOutput {
	float4 position : SV_POSITION;
	float4 color : Color;
};

float4 main(VSOutput input) : SV_TARGET
{
    return input.color;
}