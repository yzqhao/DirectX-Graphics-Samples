
// Include common HLSL code.
#include "Common.hlsl"

#define nearestSampler gsamPointWrap
#define bilinearSampler gsamLinearWrap

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

struct PixelOut
{
	float4 albedo  : SV_Target0;
    float4 normal  : SV_Target1;
    float4 specular: SV_Target2;
	float2 motion  : SV_Target3;
};

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    uint renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;

	float intensity;
	float useFadeEffect;
	float padding01;
	float padding02;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

    vout.PosH = float4(vin.PosL, 1.0f);
    vout.TexC = vin.TexC.xy;
	
    return vout;
}



