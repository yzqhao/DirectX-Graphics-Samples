
// Include common HLSL code.
#include "SSRRenderCommon.hlsl"
#include "LightingUtil.hlsl"

float4 main(VertexOutPosUv pin) : SV_TARGET
{
	float4 Out;

	TextureCube irradianceMap = girradianceMap;
	TextureCube specularMap = gSpecularMap;
	Texture2D AlbedoTexture = textureMaps[g_AlbedoTextureID];
	Texture2D NormalTexture = textureMaps[g_NormalTextureID];
	Texture2D RoughnessTexture = textureMaps[g_RoughnessTextureID];
	Texture2D DepthTexture = textureMaps[g_DepthTextureID];
	Texture2D brdfIntegrationMap = textureMaps[g_BRDFTextureID];

	float3 albedo = AlbedoTexture.Sample(gsamPointWrap, pin.TexC).rgb;
	float4 normalColor = NormalTexture.Sample(gsamPointWrap, pin.TexC);
	float4 roughnessColor = RoughnessTexture.Sample(gsamPointWrap, pin.TexC);

	float _roughness = roughnessColor.r;
	float _metalness = normalColor.a;

	float ao = 1.0f;

	float depth = DepthTexture.Sample(gsamPointWrap, pin.TexC).r;

	if(depth >= 1.0)
	{
		Out = float4(albedo, 1.0);
	}

	float3 N = normalize(normalColor.rgb);
	float2 ndc = float2(pin.TexC.x * 2.0 - 1.0, (1.0 - pin.TexC.y) * 2.0 - 1.0);
	float3 worldPos = getWorldPositionFromDepth(ndc, depth);
	float3 V = normalize(gEyePosW.xyz - worldPos);
	float3 R = reflect(-V, N);
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo, _metalness);

	float3 Lo = float3(0.0, 0.0, 0.0);
	// 方向光
	for(int i = 0; i < gAmountOfDLights; ++i)
	{
		float3 L = -normalize(gDLights[i].mDir.xyz);
		float3 H = normalize(V + L);
		float3 radiance = gDLights[i].mCol.rgb * gDLights[i].mCol.a;
		float NDF = distributionGGX(N, H, _roughness);
		float G = GeometrySmith(N, V, L, _roughness);
		float3 F = fresnelSchlick(dot(N,H), F0);
		float3 nominator = NDF * G * F;
		float denominator = 4.0f * max(dot(N,V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		float3 specular = nominator / denominator;
		float3 kS = F;
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		kD *= 1.0f - _metalness;
		float NdotL = max(dot(N, L), 0.0);
		if(NdotL>0.0f)
		{
			Lo += (kD * albedo / PI + specular) * radiance * NdotL;
		}
		else
		{
			Lo += float3(0.0f, 0.0f, 0.0f);
		}
	}

	// 点光
	for(int pl_i = 0; pl_i < gAmountOfPLights; ++pl_i)
	{
		float3 L = normalize(gPLights[pl_i].pos.xyz - worldPos);
		float3 H = normalize(V + L);
		float distance = length(gPLights[pl_i].pos.xyz - worldPos);
		float distanceByRadius = 1.0f - pow(distance / gPLights[pl_i].radius, 4);
		float clamped = pow(clamp(distanceByRadius, 0.0f, 1.0f), 2.0f);
		float attenuation = clamped / (distance * distance + 1.0f);
		float3 radiance = gPLights[pl_i].col.rgb * attenuation * gPLights[pl_i].intensity;
		float NDF = distributionGGX(N, H, _roughness);
		float G = GeometrySmith(N, V, L, _roughness);
		float3 F = fresnelSchlick(dot(N, H), F0);
		float3 nominator = NDF * G * F;
		float denominator = 4.0f * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		float3 specular = nominator / denominator;
		float3 kS = F;
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		kD *= 1.0f - _metalness;
		float NdotL = max(dot(N, L), 0.0);
		if(NdotL>0.0f) {
			Lo += (kD * albedo / PI + specular) * radiance * NdotL;
		}else {
			Lo+= float3(0.0f, 0.0f, 0.0f);
		}
	}

	float3 F = FresnelSchlickRoughness(dot(N, V), F0, _roughness);
	float3 kS = F;
	float3 kD = float3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - _metalness;
	float3 irradiance = irradianceMap.Sample(gsamLinearWrap, N).rgb;
	float3 diffuse = kD * irradiance * albedo;
	float3 specularColor = specularMap.SampleLevel(gsamLinearWrap, R, _roughness * 4).rgb;
	float2 maxNVRough = float2(max(dot(N, V), 0.0), _roughness);
	float2 brdf = brdfIntegrationMap.Sample(gsamPointWrap, maxNVRough).rg;
	float3 specular = specularColor * (F * brdf.x + brdf.y);
	float3 ambient = float3(diffuse + specular)*float3(ao,ao,ao);
	float3 color = Lo + ambient * 0.2;
    Out = float4(color.r, color.g, color.b, 1.0f);

	return (Out);
}


