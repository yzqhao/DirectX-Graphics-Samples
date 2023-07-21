#ifndef FFX_SSSR_DEPTH_DOWNSAMPLE
#define FFX_SSSR_DEPTH_DOWNSAMPLE

//ENABLE_WAVEOPS()

Texture2D<float> g_depth_buffer : register(t0, space0);
RWTexture2D<float> g_downsampled_depth_buffer[13] : register(u0, space0);
RWStructuredBuffer<uint> g_global_atomic : register(u13, space0);

//#define srt_g_group_shared_counter g_group_shared_counter

groupshared float g_group_shared_depth_values[16][16];
groupshared uint g_group_shared_counter;

// Define fetch and store functions
float4 SpdLoadSourceImage(int2 index)
{
    return g_depth_buffer.Load(int3(index, 0)).xxxx;
}
float4 SpdLoad(uint2 index)
{
    // 5 -> 6 as we store a copy of the depth buffer at index 0
    return g_downsampled_depth_buffer[6][index].xxxx;
}
void SpdStore(uint2 pix, float4 outValue, uint index)
{
    // + 1 as we store a copy of the depth buffer at index 0
    g_downsampled_depth_buffer[index + 1][pix] = outValue.x;
    //Write2D(g_downsampled_depth_buffer[index + 1], pix, outValue.x);
}
void SpdIncreaseAtomicCounter()
{
    InterlockedAdd(g_global_atomic[0], 1u, g_group_shared_counter);
}
void SpdResetAtomicCounter()
{
    //AtomicStore(g_global_atomic[0], 0);
    g_global_atomic[0] = 0;
}
uint SpdGetAtomicCounter()
{
    return g_group_shared_counter;
}
float4 SpdLoadIntermediate(uint x, uint y)
{
    float f = g_group_shared_depth_values[x][y];
    return float4(f, f, f, f);
}
void SpdStoreIntermediate(uint x, uint y, float4 value)
{
    g_group_shared_depth_values[x][y] = value.x;
}
float4 SpdReduce4(float4 v0, float4 v1, float4 v2, float4 v3)
{
    return min(min(v0, v1), min(v2, v3));
}

void SpdWorkgroupShuffleBarrier()
{
    GroupMemoryBarrier();
}

// Only last active workgroup should proceed
bool SpdExitWorkgroup(uint numWorkGroups, uint localInvocationIndex)
{
    // global atomic counter
    if (localInvocationIndex == 0)
    {
        SpdIncreaseAtomicCounter();
    }
    SpdWorkgroupShuffleBarrier();
    return (SpdGetAtomicCounter() != (numWorkGroups - 1));
}

float4 SpdReduceQuad(float4 v)
{
    // requires SM6.0
    uint quad = WaveGetLaneIndex() & (~0x3);
    float4 v0 = v;
    float4 v1 = WaveReadLaneAt(v, quad | 1);
    float4 v2 = WaveReadLaneAt(v, quad | 2);
    float4 v3 = WaveReadLaneAt(v, quad | 3);
    return SpdReduce4(v0, v1, v2, v3);
}

float4 SpdReduceIntermediate(uint2 i0, uint2 i1, uint2 i2, uint2 i3)
{
    float4 v0 = SpdLoadIntermediate(i0.x, i0.y);
    float4 v1 = SpdLoadIntermediate(i1.x, i1.y);
    float4 v2 = SpdLoadIntermediate(i2.x, i2.y);
    float4 v3 = SpdLoadIntermediate(i3.x, i3.y);
    return SpdReduce4(v0, v1, v2, v3);
}

float4 _SpdReduceLoad4(uint2 i0, uint2 i1, uint2 i2, uint2 i3)
{
    float4 v0 = SpdLoad(i0);
    float4 v1 = SpdLoad(i1);
    float4 v2 = SpdLoad(i2);
    float4 v3 = SpdLoad(i3);
    return SpdReduce4(v0, v1, v2, v3);
}

float4 SpdReduceLoad4(uint2 base)
{
    return _SpdReduceLoad4(
        uint2(base + uint2(0, 0)),
        uint2(base + uint2(0, 1)),
        uint2(base + uint2(1, 0)),
        uint2(base + uint2(1, 1)));
}

float4 _SpdReduceLoadSourceImage4(uint2 i0, uint2 i1, uint2 i2, uint2 i3)
{
    float4 v0 = SpdLoadSourceImage(int2(i0));
    float4 v1 = SpdLoadSourceImage(int2(i1));
    float4 v2 = SpdLoadSourceImage(int2(i2));
    float4 v3 = SpdLoadSourceImage(int2(i3));
    return SpdReduce4(v0, v1, v2, v3);
}

float4 SpdReduceLoadSourceImage4(uint2 base)
{
    return _SpdReduceLoadSourceImage4(
        uint2(base + uint2(0, 0)),
        uint2(base + uint2(0, 1)),
        uint2(base + uint2(1, 0)),
        uint2(base + uint2(1, 1)));
}

void SpdDownsampleMips_0_1(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip)
{
    float4 v[4];

    uint2 tex = (workGroupID.xy) * 64 + uint2(x * 2, y * 2);
    uint2 pix = (workGroupID.xy) * 32 + uint2(x, y);
    v[0] = SpdReduceLoadSourceImage4(tex);
    SpdStore(pix, v[0], 0);

    tex = (workGroupID.xy * 64) + uint2(x * 2 + 32, y * 2);
    pix = (workGroupID.xy * 32) + uint2(x + 16, y);
    v[1] = SpdReduceLoadSourceImage4(tex);
    SpdStore(pix, v[1], 0);

    tex = (workGroupID.xy * 64) + uint2(x * 2, y * 2 + 32);
    pix = (workGroupID.xy * 32) + uint2(x, y + 16);
    v[2] = SpdReduceLoadSourceImage4(tex);
    SpdStore(pix, v[2], 0);

    tex = (workGroupID.xy * 64) + uint2(x * 2 + 32, y * 2 + 32);
    pix = (workGroupID.xy * 32) + uint2(x + 16, y + 16);
    v[3] = SpdReduceLoadSourceImage4(tex);
    SpdStore(pix, v[3], 0);

    if (mip <= 1)
        return;

    v[0] = SpdReduceQuad(v[0]);
    v[1] = SpdReduceQuad(v[1]);
    v[2] = SpdReduceQuad(v[2]);
    v[3] = SpdReduceQuad(v[3]);

    if ((localInvocationIndex % 4) == 0)
    {
        SpdStore((workGroupID.xy * 16) +
            uint2(x / 2, y / 2), v[0], 1);
        SpdStoreIntermediate(
            x / 2, y / 2, v[0]);

        SpdStore((workGroupID.xy * 16) +
            uint2(x / 2 + 8, y / 2), v[1], 1);
        SpdStoreIntermediate(
            x / 2 + 8, y / 2, v[1]);

        SpdStore((workGroupID.xy * 16) +
            uint2(x / 2, y / 2 + 8), v[2], 1);
        SpdStoreIntermediate(
            x / 2, y / 2 + 8, v[2]);

        SpdStore((workGroupID.xy * 16) +
            uint2(x / 2 + 8, y / 2 + 8), v[3], 1);
        SpdStoreIntermediate(
            x / 2 + 8, y / 2 + 8, v[3]);
    }
}

void SpdDownsampleMip_2(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip)
{
    float4 v = SpdLoadIntermediate(x, y);
    v = SpdReduceQuad(v);
    // quad index 0 stores result
    if (localInvocationIndex % 4 == 0)
    {
        SpdStore((workGroupID.xy * 8) + uint2(x / 2, y / 2), v, mip);
        SpdStoreIntermediate(x + (y / 2) % 2, y, v);
    }
}

void SpdDownsampleMip_3(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip)
{
    if (localInvocationIndex < 64)
    {
        float4 v = SpdLoadIntermediate(x * 2 + y % 2, y * 2);
        v = SpdReduceQuad(v);
        // quad index 0 stores result
        if (localInvocationIndex % 4 == 0)
        {
            SpdStore((workGroupID.xy * 4) + uint2(x / 2, y / 2), v, mip);
            SpdStoreIntermediate(x * 2 + y / 2, y * 2, v);
        }
    }
}

void SpdDownsampleMip_4(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip)
{
    if (localInvocationIndex < 16)
    {
        float4 v = SpdLoadIntermediate(x * 4 + y, y * 4);
        v = SpdReduceQuad(v);
        // quad index 0 stores result
        if (localInvocationIndex % 4 == 0)
        {
            SpdStore((workGroupID.xy * 2) + uint2(x / 2, y / 2), v, mip);
            SpdStoreIntermediate(x / 2 + y, 0, v);
        }
    }
}

void SpdDownsampleMip_5(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip)
{
    if (localInvocationIndex < 4)
    {
        float4 v = SpdLoadIntermediate(localInvocationIndex, 0);
        v = SpdReduceQuad(v);
        // quad index 0 stores result
        if (localInvocationIndex % 4 == 0)
        {
            SpdStore((workGroupID.xy), v, mip);
        }
    }
}

void SpdDownsampleMips_6_7(uint x, uint y, uint mips)
{
    uint2 tex = uint2(x * 4 + 0, y * 4 + 0);
    uint2 pix = uint2(x * 2 + 0, y * 2 + 0);
    float4 v0 = SpdReduceLoad4(tex);
    SpdStore(pix, v0, 6);

    tex = uint2(x * 4 + 2, y * 4 + 0);
    pix = uint2(x * 2 + 1, y * 2 + 0);
    float4 v1 = SpdReduceLoad4(tex);
    SpdStore(pix, v1, 6);

    tex = uint2(x * 4 + 0, y * 4 + 2);
    pix = uint2(x * 2 + 0, y * 2 + 1);
    float4 v2 = SpdReduceLoad4(tex);
    SpdStore(pix, v2, 6);

    tex = uint2(x * 4 + 2, y * 4 + 2);
    pix = uint2(x * 2 + 1, y * 2 + 1);
    float4 v3 = SpdReduceLoad4(tex);
    SpdStore(pix, v3, 6);

    if (mips <= 7) return;
    // no barrier needed, working on values only from the same thread

    float4 v = SpdReduce4(v0, v1, v2, v3);
    SpdStore(uint2(x, y), v, 7);
    SpdStoreIntermediate(x, y, v);
}

void SpdDownsampleNextFour(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint baseMip, uint mips)
{
    if (mips <= baseMip) return;
    SpdWorkgroupShuffleBarrier();
    SpdDownsampleMip_2(x, y, workGroupID, localInvocationIndex, baseMip);

    if (mips <= baseMip + 1) return;
    SpdWorkgroupShuffleBarrier();
    SpdDownsampleMip_3(x, y, workGroupID, localInvocationIndex, baseMip + 1);

    if (mips <= baseMip + 2) return;
    SpdWorkgroupShuffleBarrier();
    SpdDownsampleMip_4(x, y, workGroupID, localInvocationIndex, baseMip + 2);

    if (mips <= baseMip + 3) return;
    SpdWorkgroupShuffleBarrier();
    SpdDownsampleMip_5(x, y, workGroupID, localInvocationIndex, baseMip + 3);
}

uint ABfe(uint src, uint off, uint bits)
{
    uint mask = (1u << bits) - 1; return (src >> off) & mask;
}
uint ABfiM(uint src, uint ins, uint bits)
{
    uint mask = (1u << bits) - 1; return (ins & mask) | (src & (~mask));
}
uint2 ARmpRed8x8(uint a)
{
    return uint2(ABfiM(ABfe(a, 2u, 3u), a, 1u), ABfiM(ABfe(a, 3u, 3u), ABfe(a, 1u, 2u), 2u));
}

void SpdDownsample(
    uint2 workGroupID,
    uint localInvocationIndex,
    uint mips,
    uint numWorkGroups)
{
    uint2 sub_xy = ARmpRed8x8(localInvocationIndex % 64);
    uint x = sub_xy.x + 8 * ((localInvocationIndex >> 6) % 2);
    uint y = sub_xy.y + 8 * ((localInvocationIndex >> 7));
    SpdDownsampleMips_0_1(x, y, workGroupID, localInvocationIndex, mips);

    SpdDownsampleNextFour(x, y, workGroupID, localInvocationIndex, 2, mips);

    if (mips <= 6) return;

    if (SpdExitWorkgroup(numWorkGroups, localInvocationIndex)) return;

    SpdResetAtomicCounter();

    // After mip 6 there is only a single workgroup left that downsamples the remaining up to 64x64 texels.
    SpdDownsampleMips_6_7(x, y, mips);

    SpdDownsampleNextFour(x, y, uint2(0, 0), localInvocationIndex, 8, mips);
}

uint GetThreadgroupCount(int2 image_size)
{
    // Each threadgroup works on 64x64 texels
    return ((image_size.x + 63) / 64) * ((image_size.y + 63) / 64);
}

// Returns mips count of a texture with specified size
float GetMipsCount(int2 texture_size)
{
    float max_dim = float(max(texture_size.x, texture_size.y));
    return 1.0 + floor(log2(max_dim));
}

[numthreads(32, 8, 1)]
void csmain(uint3 did : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gi : SV_GroupIndex)
{
    int2 depth_image_size;
    g_depth_buffer.GetDimensions(depth_image_size.x, depth_image_size.y);

    // Copy most detailed level into the hierarchy and transform it.
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            int2 idx = int2(2 * did.x + i, 8 * did.y + j);
            if (idx.x < depth_image_size.x && idx.y < depth_image_size.y)
            {
                g_downsampled_depth_buffer[0][idx] = g_depth_buffer.Load(int3(idx, 0));
                //Write2D(Get(g_downsampled_depth_buffer)[0], idx, LoadTex2D(Get(g_depth_buffer), NO_SAMPLER, idx, 0));
            }
        }
    }

    int2 image_size;
    g_downsampled_depth_buffer[0].GetDimensions(image_size.x, image_size.y);
    float mips_count = GetMipsCount(image_size);
    uint threadgroup_count = GetThreadgroupCount(image_size);

    SpdDownsample(
        uint2(gid.xy),
        uint(gi),
        uint(mips_count),
        uint(threadgroup_count));
}
#endif // FFX_SSSR_DEPTH_DOWNSAMPLE