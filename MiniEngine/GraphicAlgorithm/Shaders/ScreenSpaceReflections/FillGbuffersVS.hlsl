
#include "SSRRenderCommon.hlsl"

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : TEXCOORD2;
    float4 CurPos  : TEXCOORD3;
    float4 PrePos  : TEXCOORD4;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
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

VertexOut main(VertexPosUvNormal vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    // Transform to world space.
    float4 posW = mul(worldMat, float4(vin.PosL, 1.0f));
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = normalize(mul(worldMat, float4(vin.NormalL, 0.0f)).xyz);

    // Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, posW);

    vout.CurPos = vout.PosH;
    vout.PrePos = mul(gPreViewProj, mul(prevWorldMat, float4(vin.PosL, 1.0f)));
	

    vout.TexC = vin.TexC.xy;
	
    return vout;
}

