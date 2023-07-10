

Texture2D<float> Source : register(t0, space0);
RWTexture2D<float> Destination : register(u0, space0);

[numthreads(8, 8, 1)]
void csmain(uint3 did : SV_DispatchThreadID)
{
    uint2 screen_size;
    Source.GetDimensions(screen_size.x, screen_size.y);

    if (did.x < screen_size.x && did.y < screen_size.y)
    {
        Destination[did.xy] = Source.Load(int3(did.xy, 0));
    }
}
