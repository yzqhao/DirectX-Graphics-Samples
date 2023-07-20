
#include "SSRRenderCommon.hlsl"

struct VertexOut
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 worldMat;
	float4x4 prevWorldMat;
	uint gMaterialIndex;
	float roughness;
	float metalness;
	int pbrMaterials;
};

GBufferPixelOut main(VertexOut pin)
{
	GBufferPixelOut Out;
	Out.albedo = gCubeMap.Sample(gsamLinearWrap, pin.PosL);
	return Out;
}

