struct RibbonSegmentPayload
{
    float4 previousPosition;
    float4 currentPosition;
    float4 nextPosition;
    float4 nextNextPosition;
    float4 currentSampleParams; // x: age, y: lifetime, z: distance, w: widthScale
    float4 nextSampleParams;    // x: age, y: lifetime, z: distance, w: widthScale
    float4 startColor;
    float4 endColor;
    float4 subUVRect;
    float4 coreColorRgb;
};

struct RibbonInstanceVertex
{
    float4 previousPosition;
    float4 currentPosition;
    float4 nextPosition;
    float4 nextNextPosition;
    float2 lifeTime;
    float4 startColor;
    float4 endColor;
    float4 subUVRect;
    float4 segmentParams;
    float4 coreColorRgb;
};

struct DrawIndexedInstancedIndirectArgs
{
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer RibbonComputeParams : register(b0)
{
    uint g_SegmentCount;
    uint g_RenderAxis;
    float g_TilingDistance;
    float g_VisibleLength;
    float4 g_AxisFallback;
    float4 g_FadeParams; // x: auto life fade, y: max length, z: tail fade length, w: reserved
};

StructuredBuffer<RibbonSegmentPayload> SegmentData : register(t0);
RWStructuredBuffer<RibbonInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> ArgsData : register(u1);

[numthreads(64, 1, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;

    if (index == 0u)
    {
        ArgsData[0].indexCountPerInstance = 1u;
        ArgsData[0].instanceCount = g_SegmentCount;
        ArgsData[0].startIndexLocation = 0u;
        ArgsData[0].baseVertexLocation = 0;
        ArgsData[0].startInstanceLocation = 0u;
    }

    if (index >= g_SegmentCount)
        return;

    const RibbonSegmentPayload segment = SegmentData[index];
    const float age = max(0.0f, segment.currentSampleParams.x);
    const float lifetime = max(0.0001f, segment.currentSampleParams.y);
    const float lifeProgress = saturate(age / lifetime);
    const float currentDistance = segment.currentSampleParams.z;
    const float nextDistance = segment.nextSampleParams.z;
    const float visibleLength = max(g_VisibleLength, 0.0001f);
    const float currentV = g_TilingDistance > 0.0f ? currentDistance / max(g_TilingDistance, 0.0001f) : currentDistance / visibleLength;
    const float nextV = g_TilingDistance > 0.0f ? nextDistance / max(g_TilingDistance, 0.0001f) : nextDistance / visibleLength;
    RibbonInstanceVertex output = (RibbonInstanceVertex)0;
    output.previousPosition = segment.previousPosition;
    output.currentPosition = segment.currentPosition;
    output.nextPosition = segment.nextPosition;
    output.nextNextPosition = segment.nextNextPosition;
    output.lifeTime = float2(lifetime, age);
    output.startColor = segment.startColor;
    output.endColor = segment.endColor;
    const float subUVHeight = segment.subUVRect.w - segment.subUVRect.y;
    output.subUVRect = float4(
        segment.subUVRect.x,
        segment.subUVRect.y + currentV * subUVHeight,
        segment.subUVRect.z,
        segment.subUVRect.y + nextV * subUVHeight
    );
    output.segmentParams = float4(
        lifeProgress,
        max(segment.currentSampleParams.w, 0.001f),
        max(segment.nextSampleParams.w, 0.001f),
        1.0f
    );
    output.coreColorRgb = segment.coreColorRgb;
    OutputData[index] = output;
}
