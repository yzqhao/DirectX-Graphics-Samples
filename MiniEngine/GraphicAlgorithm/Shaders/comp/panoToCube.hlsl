#include "../StaticSample.hlsl"

cbuffer RootConstant : register(b0)
{
	uint mip;
	uint maxSize;
};

Texture2D<float4> srcTexture : register(t0, space0);
RWTexture2DArray<float4> dstTexture[11] : register(u0, space0);

//RES(SamplerState, skyboxSampler, UPDATE_FREQ_NONE, s3, binding = 2);

[numthreads(16, 16, 1)]
void csmain(uint3 DTid : SV_DispatchThreadID) 
{
	float2 invAtan = float2(0.1591f, 0.3183f);

	float3 threadPos = float3(DTid);

	{
		uint mipSize = maxSize >> mip;
		if (threadPos.x >= mipSize || threadPos.y >= mipSize)
			return;

		float2 texcoords = float2(float(threadPos.x + 0.5) / mipSize, 1.0f - float(threadPos.y + 0.5) / mipSize);
		float3 sphereDir = float3(0, 0, 0);
		if (threadPos.z <= 0)
			sphereDir = normalize(float3(0.5, -(texcoords.y - 0.5), -(texcoords.x - 0.5)));
		else if (threadPos.z <= 1)
			sphereDir = normalize(float3(-0.5, -(texcoords.y - 0.5), texcoords.x - 0.5));
		else if (threadPos.z <= 2)
			sphereDir = normalize(float3(texcoords.x - 0.5, -0.5, -(texcoords.y - 0.5)));
		else if (threadPos.z <= 3)
			sphereDir = normalize(float3(texcoords.x - 0.5, 0.5, texcoords.y - 0.5));
		else if (threadPos.z <= 4)
			sphereDir = normalize(float3(texcoords.x - 0.5, -(texcoords.y - 0.5), 0.5));
		else if (threadPos.z <= 5)
			sphereDir = normalize(float3(-(texcoords.x - 0.5), -(texcoords.y - 0.5), -0.5));

		float2 panoUVs = float2(atan2(sphereDir.z, sphereDir.x), asin(sphereDir.y));
		panoUVs *= invAtan;
		panoUVs += 0.5f;

		// We need to use SampleLevel, since compute shaders do not support regular sampling 
        float4 skycolor = srcTexture.SampleLevel(gsamLinearWrap, panoUVs, mip);
		dstTexture[mip][threadPos] = skycolor;
		//Write3D(Get(dstTexture), int3(threadPos), SampleLvlTex2D(Get(srcTexture), Get(skyboxSampler), panoUVs, float(Get(mip))));
	}
}
