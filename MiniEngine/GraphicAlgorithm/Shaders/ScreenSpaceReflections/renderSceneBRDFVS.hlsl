
// Include common HLSL code.
#include "../Common.hlsl"

VertexOutPosUv main(VertexPosUv vin)
{
    VertexOutPosUv vout = (VertexOutPosUv)0.0f;

    vout.PosH = float4(vin.PosL, 1.0f);
    vout.TexC = vin.TexC.xy;
	
    return vout;
}



