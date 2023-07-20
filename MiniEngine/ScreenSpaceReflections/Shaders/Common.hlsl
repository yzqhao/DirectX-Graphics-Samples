//***************************************************************************************
// Common.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************
#ifndef _COMMON
#define _COMMON

#define MaxLights 16

#include "StaticSample.hlsl"

struct DirectionalLight
{
	float4 mPos;
	float4 mCol; //alpha is the intesity
	float4 mDir;
};

struct PointLight
{
	float4 pos;
	float4 col;
	float radius;
	float intensity;
	float _pad0;
	float _pad1;
};

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gPreViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gViewPortSize;
    float4 gAmbientLight;

    DirectionalLight gDLights[MaxLights];
    PointLight gPLights[MaxLights];
    int gAmountOfDLights;
    int gAmountOfPLights;
};

struct VertexOnlyPos
{
	float3 PosL    : POSITION;
};

struct VertexPosUv
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexPosUvNormal
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
    float3 NormalL : NORMAL;
};

struct VertexPosUvNormalTang
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
    float3 NormalL : NORMAL;
    float3 Tangent : TANGENT;
};

struct VertexOutPosUv
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

struct GBufferPixelOut
{
	float4 albedo  : SV_Target0;
    float4 normal  : SV_Target1;
    float4 specular: SV_Target2;
	float2 motion  : SV_Target3;
};

#endif  // _COMMON
