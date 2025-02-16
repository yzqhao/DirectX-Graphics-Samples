
#include "Common.hlsl"

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
 
VertexOut main(VertexOnlyPos vin)
{
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;
	
	// Transform to world space.
	float4 posW = float4(vin.PosL, 1.0f);

	// Always center sky about camera.
	posW.xyz += gEyePosW;
	
	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(gViewProj, posW).xyww;
	
	return vout;
}

