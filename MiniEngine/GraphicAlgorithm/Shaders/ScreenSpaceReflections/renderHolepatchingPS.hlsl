
// Include common HLSL code.
#include "SSRRenderCommon.hlsl"

#define nearestSampler gsamPointWrap
#define bilinearSampler gsamLinearWrap

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

float4 main(VertexOutPosUv pin) : SV_TARGET
{
	float4 outColor;
	outColor = float4(0.0, 0.0, 0.0, 0.0);

	Texture2D SceneTexture = textureMaps[90];
	Texture2D SSRTexture = textureMaps[2];

	float4 ssrColor = SSRTexture.Sample(nearestSampler, pin.TexC);	
	
	if(renderMode == 0)
	{
		outColor = SceneTexture.Sample(bilinearSampler, pin.TexC);
		outColor = outColor / (outColor + float4(1,1,1,1));

		outColor.w = 1.0f;
		return (outColor);
	}		
	else if(renderMode == 1)
	{
		outColor = float4(0.0, 0.0, 0.0, 0.0);
	}
	
	if(useHolePatching < 0.5)
	{
		outColor.w = 1.0;

		if(ssrColor.w > 0.0)
		{
			outColor = ssrColor;
		}
	}
	else if(ssrColor.w > 0.0)
	{
		float threshold = ssrColor.w;
		float minOffset = threshold;
		

		float4 neighborColor00 = SSRTexture.Sample(nearestSampler, pin.TexC + float2(1.0/gViewPortSize.x, 0.0));
		float4 neighborColorB00 = SSRTexture.Sample(bilinearSampler, pin.TexC + float2(1.0/gViewPortSize.x, 0.0));
		if(neighborColor00.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor00.w);			
		}

		float4 neighborColor01 = SSRTexture.Sample(nearestSampler, pin.TexC - float2(1.0/gViewPortSize.x, 0.0));
		float4 neighborColorB01 = SSRTexture.Sample(bilinearSampler, pin.TexC - float2(1.0/gViewPortSize.x, 0.0));
		if(neighborColor01.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor01.w);			
		}

		float4 neighborColor02 = SSRTexture.Sample(nearestSampler, pin.TexC + float2(0.0, 1.0/gViewPortSize.y));
		float4 neighborColorB02 = SSRTexture.Sample(bilinearSampler, pin.TexC + float2(0.0, 1.0/gViewPortSize.y));
		if(neighborColor02.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor02.w);			
		}

		float4 neighborColor03 = SSRTexture.Sample(nearestSampler, pin.TexC - float2(0.0, 1.0/gViewPortSize.y));
		float4 neighborColorB03 = SSRTexture.Sample(bilinearSampler, pin.TexC - float2(0.0, 1.0/gViewPortSize.y));
		if(neighborColor03.w > 0.0)
		{
			minOffset = min(minOffset, neighborColor03.w);			
		}

		float4 neighborColor04 = SSRTexture.Sample(nearestSampler, pin.TexC + float2(1.0/gViewPortSize.x, 1.0/gViewPortSize.y));
		float4 neighborColorB04 = SSRTexture.Sample(bilinearSampler, pin.TexC + float2(1.0/gViewPortSize.x, 1.0/gViewPortSize.y));

		float4 neighborColor05 = SSRTexture.Sample(nearestSampler, pin.TexC + float2(1.0/gViewPortSize.x, -1.0/gViewPortSize.y));
		float4 neighborColorB05 = SSRTexture.Sample(bilinearSampler, pin.TexC +float2(1.0/gViewPortSize.x, -1.0/gViewPortSize.y));

		float4 neighborColor06 = SSRTexture.Sample(nearestSampler, pin.TexC + float2(-1.0/gViewPortSize.x, 1.0/gViewPortSize.y));
		float4 neighborColorB06 = SSRTexture.Sample(bilinearSampler, pin.TexC + float2(-1.0/gViewPortSize.x, 1.0/gViewPortSize.y));

		float4 neighborColor07 = SSRTexture.Sample(nearestSampler, pin.TexC - float2(1.0/gViewPortSize.x, 1.0/gViewPortSize.y));
		float4 neighborColorB07 = SSRTexture.Sample(bilinearSampler, pin.TexC - float2(1.0/gViewPortSize.x, 1.0/gViewPortSize.y));

		bool bUseExpensiveHolePatching = useExpensiveHolePatching > 0.5;

		if(bUseExpensiveHolePatching)
		{
			if(neighborColor04.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor04.w);			
			}

			if(neighborColor05.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor05.w);			
			}

			if(neighborColor06.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor06.w);			
			}
				
			if(neighborColor07.w > 0.0)
			{
				minOffset = min(minOffset, neighborColor07.w);			
			}
		}

		float blendValue = 0.5;

		if(bUseExpensiveHolePatching)
		{
			if(minOffset == neighborColor00.w)
			{
					outColor =  lerp(neighborColor00, neighborColorB00, blendValue);
			}
			else if(minOffset == neighborColor01.w)
			{
					outColor = lerp(neighborColor01, neighborColorB01, blendValue);
			}
			else if(minOffset == neighborColor02.w)
			{
					outColor = lerp(neighborColor02, neighborColorB02, blendValue);
			}
			else if(minOffset == neighborColor03.w)
			{
					outColor = lerp(neighborColor03, neighborColorB03, blendValue);
			}
			else if(minOffset == neighborColor04.w)
			{
					outColor = lerp(neighborColor04, neighborColorB04, blendValue);
			}
			else if(minOffset == neighborColor05.w)
			{
					outColor = lerp(neighborColor05, neighborColorB05, blendValue);
			}
			else if(minOffset == neighborColor06.w)
			{
					outColor = lerp(neighborColor06, neighborColorB06, blendValue);
			}
			else if(minOffset == neighborColor07.w)
			{
					outColor = lerp(neighborColor07, neighborColorB07, blendValue);
			}
		}
		else
		{
			if(minOffset == neighborColor00.w)
			{
					outColor = lerp(neighborColor00, neighborColorB00, blendValue);
			}
			else if(minOffset == neighborColor01.w)
			{
					outColor = lerp(neighborColor01, neighborColorB01, blendValue);
			}
			else if(minOffset == neighborColor02.w)
			{
					outColor = lerp(neighborColor02, neighborColorB02, blendValue);
			}
			else if(minOffset == neighborColor03.w)
			{
					outColor = lerp(neighborColor03, neighborColorB03, blendValue);
			}
		}
		
		if(minOffset <= threshold)
			outColor.w = 1.0;
		else
			outColor.w = 0.0;
	}
	
	if(renderMode == 3)
	{
		if(ssrColor.w <= 0.0)
			outColor = SceneTexture.Sample(bilinearSampler, pin.TexC);
	}

	if(renderMode == 2)
	{
		outColor = outColor * intensity + SceneTexture.Sample(bilinearSampler, pin.TexC);	
	}

	outColor = outColor / (outColor + float4(1.0f, 1.0f, 1.0f, 1.0f));
	outColor.w = 1.0f;

	return outColor;
}


