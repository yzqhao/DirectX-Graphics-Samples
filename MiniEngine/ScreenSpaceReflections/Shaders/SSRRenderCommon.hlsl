#ifndef _SSR_RENDER_COMMON_
#define _SSR_RENDER_COMMON_

#include "Common.hlsl"

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

#endif