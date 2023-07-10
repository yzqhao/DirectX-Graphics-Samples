#include "../StaticSample.hlsl"

#ifndef SAMPLE_DELTA
	#define SAMPLE_DELTA 0.25f
#endif

static const float Pi = 3.14159265359;
static float SampleDelta = SAMPLE_DELTA;

TextureCube<float4> srcTexture : register(t0, space0);
RWTexture2DArray<float4> dstTexture : register(u0, space0);
//SamplerState skyboxSampler : register(s3, space0)

#define skyboxSampler gsamLinearWrap

float4 computeIrradiance(float3 N)
{
	float4 irradiance = float4(0.0, 0.0, 0.0, 0.0);

	float3 up = float3(0.0, 1.0, 0.0);
	float3 right = cross(up, N);
	up = cross(N, right);

	float nrSamples = 0.0;

	for (float phi = 0.0; phi < 2.0 * Pi; phi += SampleDelta)
	{
		for (float theta = 0.0; theta < 0.5 * Pi; theta += SampleDelta)
		{
			// spherical to cartesian (in tangent space)
			float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

			// tangent space to world
			float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;


			float4 sampledValue = srcTexture.SampleLevel(skyboxSampler, sampleVec, 0);

			irradiance += float4(sampledValue.rgb * cos(theta) * sin(theta), sampledValue.a);
			nrSamples++;

		}
	}

	return Pi * irradiance * (1.0 / float(nrSamples));
}

[numthreads(16, 16, 1)]
void csmain(int3 DTid : SV_DispatchThreadID)
{
	float3 threadPos = float3(DTid);
	uint pixelOffset = 0;

	for (uint i = 0; i < threadPos.z; ++i)
	{
		pixelOffset += 32 * 32;
	}

	float2 texcoords = float2(float(threadPos.x + 0.5) / 32.0f, float(threadPos.y + 0.5) / 32.0f);

	float3 sphereDir = float3(1, 1, 1);

	if (threadPos.z <= 0) {
		sphereDir = normalize(float3(0.5, -(texcoords.y - 0.5), -(texcoords.x - 0.5)));
	}
	else if (threadPos.z <= 1) {
		sphereDir = normalize(float3(-0.5, -(texcoords.y - 0.5), texcoords.x - 0.5));
	}
	else if (threadPos.z <= 2) {
		sphereDir = normalize(float3(texcoords.x - 0.5, 0.5, texcoords.y - 0.5));
	}
	else if (threadPos.z <= 3) {
		sphereDir = normalize(float3(texcoords.x - 0.5, -0.5, -(texcoords.y - 0.5)));
	}
	else if (threadPos.z <= 4) {
		sphereDir = normalize(float3(texcoords.x - 0.5, -(texcoords.y - 0.5), 0.5));
	}
	else if (threadPos.z <= 5) {
		sphereDir = normalize(float3(-(texcoords.x - 0.5), -(texcoords.y - 0.5), -0.5));
	}

	// uint pixelId = uint(pixelOffset + threadPos.y * 32 + threadPos.x);

	float4 irradiance = computeIrradiance(sphereDir);

	dstTexture[threadPos] = irradiance;
}
