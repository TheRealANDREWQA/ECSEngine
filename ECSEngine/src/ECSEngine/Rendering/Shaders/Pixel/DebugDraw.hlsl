struct PS_INPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

float4 main(in PS_INPUT input) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}