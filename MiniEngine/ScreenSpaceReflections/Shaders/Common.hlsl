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

struct MaterialData
{
    uint textureMapIds;
};

TextureCube gCubeMap : register(t0);     // 0-skymap 
TextureCube girradianceMap : register(t1);     // 1-irradianceMap
TextureCube gSpecularMap : register(t2);     // 2-specularMap

// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D textureMaps[200] : register(t3);   // 84 material tex + 4 gbuffer + 1 depth + 1 brdf lut

static const int g_AlbedoTextureID = 85;
static const int g_NormalTextureID = 86;
static const int g_RoughnessTextureID = 87;
static const int g_MationTextureID = 88;
static const int g_DepthTextureID = 89;
static const int g_BRDFTextureID = 90;

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

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

struct GBufferPixelOut
{
	float4 albedo  : SV_Target0;
    float4 normal  : SV_Target1;
    float4 specular: SV_Target2;
	float2 motion  : SV_Target3;
};

#endif  // _COMMON
