struct TrailSamplePayload
{
    float4 baseWorldPosition;
    float4 tipWorldPosition;
    float4 sampleParams; // x: 수명, y: head 기준 누적 거리
    float4 coreColorRgb;
};

struct TrailInstanceVertex
{
    float4 right;
    float4 up;
    float4 look;
    float4 translation;
    float2 lifeTime;
    float4 startColor;
    float4 endColor;
    float4 subUVRect;
    float4 segmentParams; // x: 수명 진행률, y: 폭 배율, z: 알파 배율, w: 예비
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

cbuffer TrailComputeParams : register(b0)
{
    uint g_SegmentCount;
    float g_SegmentLifetime;
    float g_Width;
    float g_UVTiling;
    float g_SideFade;
    float3 g_TrailParamsPadding;
    float4 g_StartColor;
    float4 g_EndColor;
    float4 g_ColorOverLifeCurveParams;           // x: color key count, y: alpha key count, z: color enabled, w: alpha enabled
    float4 g_ColorOverLifeColorCurveTimes;
    float4 g_ColorOverLifeColorCurveTimesBlock1; // 커브 Key를 4개 -> 8개로 확장하면서 Block1들 추가됨
    float4 g_ColorOverLifeColorCurveValuesR;
    float4 g_ColorOverLifeColorCurveValuesRBlock1;
    float4 g_ColorOverLifeColorCurveValuesG;
    float4 g_ColorOverLifeColorCurveValuesGBlock1;
    float4 g_ColorOverLifeColorCurveValuesB;
    float4 g_ColorOverLifeColorCurveValuesBBlock1;
    float4 g_ColorOverLifeColorCurveArriveR;
    float4 g_ColorOverLifeColorCurveArriveRBlock1;
    float4 g_ColorOverLifeColorCurveArriveG;
    float4 g_ColorOverLifeColorCurveArriveGBlock1;
    float4 g_ColorOverLifeColorCurveArriveB;
    float4 g_ColorOverLifeColorCurveArriveBBlock1;
    float4 g_ColorOverLifeColorCurveLeaveR;
    float4 g_ColorOverLifeColorCurveLeaveRBlock1;
    float4 g_ColorOverLifeColorCurveLeaveG;
    float4 g_ColorOverLifeColorCurveLeaveGBlock1;
    float4 g_ColorOverLifeColorCurveLeaveB;
    float4 g_ColorOverLifeColorCurveLeaveBBlock1;
    float4 g_ColorOverLifeColorCurveModes;
    float4 g_ColorOverLifeColorCurveModesBlock1;
    float4 g_ColorOverLifeAlphaCurveTimes;
    float4 g_ColorOverLifeAlphaCurveTimesBlock1;
    float4 g_ColorOverLifeAlphaCurveValues;
    float4 g_ColorOverLifeAlphaCurveValuesBlock1;
    float4 g_ColorOverLifeAlphaCurveArrive;
    float4 g_ColorOverLifeAlphaCurveArriveBlock1;
    float4 g_ColorOverLifeAlphaCurveLeave;
    float4 g_ColorOverLifeAlphaCurveLeaveBlock1;
    float4 g_ColorOverLifeAlphaCurveModes;
    float4 g_ColorOverLifeAlphaCurveModesBlock1;
    float4 g_SizeByLifeParams;                   // x/y: width start/end, z: enabled, w: reserved
    float4 g_SizeByLifeCurveTimes;               // scaleOverLife key times
    float4 g_SizeByLifeCurveTimesBlock1;
    float4 g_SizeByLifeCurveValuesX;             // scaleOverLife X multipliers
    float4 g_SizeByLifeCurveValuesXBlock1;
    float4 g_SizeByLifeCurveArriveTangentsX;
    float4 g_SizeByLifeCurveArriveTangentsXBlock1;
    float4 g_SizeByLifeCurveLeaveTangentsX;
    float4 g_SizeByLifeCurveLeaveTangentsXBlock1;
    float4 g_SizeByLifeCurveModes;
    float4 g_SizeByLifeCurveModesBlock1;
    float4 g_SizeByLifeCurveParams;              // x: key count, y: multiplyX, zw: reserved
    float4 g_LengthParams;                       // x: maxTrailLength, y: tailFadeLength, z: autoLifeFade flag, w: reserved
};

StructuredBuffer<TrailSamplePayload> HistoryData : register(t0);
RWStructuredBuffer<TrailInstanceVertex> OutputData : register(u0);
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> ArgsData : register(u1);

static const uint kEffectDistributionCurveMaxKeys = 8u;

void ApplyTrailWidth(inout float3 basePosition, inout float3 tipPosition, float widthScale)
{
    const float3 center = (basePosition + tipPosition) * 0.5f;
    const float3 halfSpan = (tipPosition - basePosition) * 0.5f;
    if (length(halfSpan) <= 0.0001f)
        return;

    const float widthRatio = max(g_Width * max(widthScale, 0.0f), 0.0001f);
    basePosition = center - halfSpan * widthRatio;
    tipPosition = center + halfSpan * widthRatio;
}

float ReadVec4Component(float4 values, uint index)
{
    float result = values.x;
    if (index == 1u)
        result = values.y;
    else if (index == 2u)
        result = values.z;
    else if (index == 3u)
        result = values.w;

    return result;
}

float ReadVec4Component(float4 valuesBlock0, float4 valuesBlock1, uint index)
{
    return index < 4u
           ? ReadVec4Component(valuesBlock0, index)
           : ReadVec4Component(valuesBlock1, index - 4u);
}

float EvaluateCompactCurve(
    float lifeProgress,
    uint keyCount,
    float4 times,
    float4 timesBlock1,
    float4 values,
    float4 valuesBlock1,
    float4 arriveTangents,
    float4 arriveTangentsBlock1,
    float4 leaveTangents,
    float4 leaveTangentsBlock1,
    float4 modes,
    float4 modesBlock1,
    float fallbackValue)
{
    keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));

    bool found = false;
    float result = ReadVec4Component(values, valuesBlock1, 0u);
    if (keyCount > 1u && lifeProgress > ReadVec4Component(times, timesBlock1, 0u))
    {
        result = fallbackValue;
        [loop]
        for (uint keyIndex = 1u; keyIndex < kEffectDistributionCurveMaxKeys; ++keyIndex)
        {
            if (keyIndex >= keyCount || found)
                break;

            const float rightTime = ReadVec4Component(times, timesBlock1, keyIndex);
            if (lifeProgress > rightTime)
                continue;

            const float leftTime = ReadVec4Component(times, timesBlock1, keyIndex - 1u);
            const float leftValue = ReadVec4Component(values, valuesBlock1, keyIndex - 1u);
            const float rightValue = ReadVec4Component(values, valuesBlock1, keyIndex);
            const float mode = ReadVec4Component(modes, modesBlock1, keyIndex - 1u);
            const float width = max(0.0001f, rightTime - leftTime);
            const float ratio = saturate((lifeProgress - leftTime) / width);

            if (mode < 0.5f)
                result = leftValue;
            else if (mode >= 1.5f)
            {
                const float t2 = ratio * ratio;
                const float t3 = t2 * ratio;
                const float leftLeave = ReadVec4Component(leaveTangents, leaveTangentsBlock1, keyIndex - 1u) * width;
                const float rightArrive = ReadVec4Component(arriveTangents, arriveTangentsBlock1, keyIndex) * width;
                result =
                    (2.0f * t3 - 3.0f * t2 + 1.0f) * leftValue +
                    (t3 - 2.0f * t2 + ratio) * leftLeave +
                    (-2.0f * t3 + 3.0f * t2) * rightValue +
                    (t3 - t2) * rightArrive;
            }
            else
                result = lerp(leftValue, rightValue, ratio);

            found = true;
            break;
        }

        const float lastTime = ReadVec4Component(times, timesBlock1, keyCount - 1u);
        if (!found && lifeProgress > lastTime)
            result = ReadVec4Component(values, valuesBlock1, keyCount - 1u);
    }

    return result;
}

float4 EvaluateColorOverLife(float lifeProgress, float4 fallbackColor)
{
    float4 result = fallbackColor;

    if (g_ColorOverLifeCurveParams.z != 0.0f)
    {
        const uint colorKeyCount = (uint)g_ColorOverLifeCurveParams.x;
        result.rgb = saturate(
            float3(
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesR,
                    g_ColorOverLifeColorCurveValuesRBlock1,
                    g_ColorOverLifeColorCurveArriveR,
                    g_ColorOverLifeColorCurveArriveRBlock1,
                    g_ColorOverLifeColorCurveLeaveR,
                    g_ColorOverLifeColorCurveLeaveRBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.r
                ),
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesG,
                    g_ColorOverLifeColorCurveValuesGBlock1,
                    g_ColorOverLifeColorCurveArriveG,
                    g_ColorOverLifeColorCurveArriveGBlock1,
                    g_ColorOverLifeColorCurveLeaveG,
                    g_ColorOverLifeColorCurveLeaveGBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.g
                ),
                EvaluateCompactCurve(
                    lifeProgress,
                    colorKeyCount,
                    g_ColorOverLifeColorCurveTimes,
                    g_ColorOverLifeColorCurveTimesBlock1,
                    g_ColorOverLifeColorCurveValuesB,
                    g_ColorOverLifeColorCurveValuesBBlock1,
                    g_ColorOverLifeColorCurveArriveB,
                    g_ColorOverLifeColorCurveArriveBBlock1,
                    g_ColorOverLifeColorCurveLeaveB,
                    g_ColorOverLifeColorCurveLeaveBBlock1,
                    g_ColorOverLifeColorCurveModes,
                    g_ColorOverLifeColorCurveModesBlock1,
                    fallbackColor.b
                )
            )
        );
    }

    if (g_ColorOverLifeCurveParams.w != 0.0f)
    {
        const uint alphaKeyCount = (uint)g_ColorOverLifeCurveParams.y;
        result.a = saturate(
            EvaluateCompactCurve(
                lifeProgress,
                alphaKeyCount,
                g_ColorOverLifeAlphaCurveTimes,
                g_ColorOverLifeAlphaCurveTimesBlock1,
                g_ColorOverLifeAlphaCurveValues,
                g_ColorOverLifeAlphaCurveValuesBlock1,
                g_ColorOverLifeAlphaCurveArrive,
                g_ColorOverLifeAlphaCurveArriveBlock1,
                g_ColorOverLifeAlphaCurveLeave,
                g_ColorOverLifeAlphaCurveLeaveBlock1,
                g_ColorOverLifeAlphaCurveModes,
                g_ColorOverLifeAlphaCurveModesBlock1,
                fallbackColor.a
            )
        );
    }

    return result;
}

float EvaluateSizeByLifeWidthScale(float lifeProgress)
{
    const uint keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, (uint)g_SizeByLifeCurveParams.x));
    float multiplier = 1.0f;
    if (g_SizeByLifeCurveParams.y != 0.0f)
    {
        multiplier = EvaluateCompactCurve(
            lifeProgress,
            keyCount,
            g_SizeByLifeCurveTimes,
            g_SizeByLifeCurveTimesBlock1,
            g_SizeByLifeCurveValuesX,
            g_SizeByLifeCurveValuesXBlock1,
            g_SizeByLifeCurveArriveTangentsX,
            g_SizeByLifeCurveArriveTangentsXBlock1,
            g_SizeByLifeCurveLeaveTangentsX,
            g_SizeByLifeCurveLeaveTangentsXBlock1,
            g_SizeByLifeCurveModes,
            g_SizeByLifeCurveModesBlock1,
            lerp(g_SizeByLifeParams.x, g_SizeByLifeParams.y, lifeProgress)
        );
    }

    return multiplier;
}

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

    const TrailSamplePayload current = HistoryData[index];
    const TrailSamplePayload next = HistoryData[index + 1u];
    float3 currentBase = current.baseWorldPosition.xyz;
    float3 currentTip = current.tipWorldPosition.xyz;
    float3 nextBase = next.baseWorldPosition.xyz;
    float3 nextTip = next.tipWorldPosition.xyz;
    const float age = max(0.0f, current.sampleParams.x);
    const float lifeProgress = saturate(age / max(g_SegmentLifetime, 0.0001f));
    const float nextAge = max(0.0f, next.sampleParams.x);
    const float nextLifeProgress = saturate(nextAge / max(g_SegmentLifetime, 0.0001f));
    const float ageAlpha = saturate(1.0f - lifeProgress);
    const float fallbackWidthScale = lerp(0.25f, 1.0f, ageAlpha);
    const float sizeByLifeWidthScale = EvaluateSizeByLifeWidthScale(lifeProgress);
    const float widthScale = g_SizeByLifeParams.z != 0.0f ? sizeByLifeWidthScale : fallbackWidthScale;
    const float nextAgeAlpha = saturate(1.0f - nextLifeProgress);
    const float nextFallbackWidthScale = lerp(0.25f, 1.0f, nextAgeAlpha);
    const float nextSizeByLifeWidthScale = EvaluateSizeByLifeWidthScale(nextLifeProgress);
    const float nextWidthScale = g_SizeByLifeParams.z != 0.0f ? nextSizeByLifeWidthScale : nextFallbackWidthScale;
    const float currentDistance = current.sampleParams.y;
    const float nextDistance = next.sampleParams.y;
    ApplyTrailWidth(currentBase, currentTip, widthScale);
    ApplyTrailWidth(nextBase, nextTip, nextWidthScale);
    float lengthAlpha = 1.0f;
    if (g_LengthParams.x > 0.0f)
    {
        const float fadeLength = max(g_LengthParams.y, 0.001f);
        const float fadeStart = max(0.0f, g_LengthParams.x - fadeLength);
        lengthAlpha = saturate((g_LengthParams.x - currentDistance) / max(g_LengthParams.x - fadeStart, 0.001f));
    }

    TrailInstanceVertex output = (TrailInstanceVertex)0;
    output.right = float4(currentBase, 1.f);
    output.up = float4(currentTip, 1.f);
    output.look = float4(nextBase, 1.f);
    output.translation = float4(nextTip, 1.f);
    output.lifeTime = float2(max(g_SegmentLifetime, 0.0001f), age);
    const float4 segmentColor = EvaluateColorOverLife(lifeProgress, lerp(g_StartColor, g_EndColor, lifeProgress));
    output.startColor = segmentColor;
    output.endColor = segmentColor;
    output.subUVRect = float4(0.0f, currentDistance, 1.0f, nextDistance);
    const float visualAgeAlpha = g_LengthParams.z != 0.0f ? ageAlpha : 1.0f;
    output.segmentParams = float4(lifeProgress, 0.0f, visualAgeAlpha * lengthAlpha, g_SideFade); // x: 수명 진행률, y: reserved, z: 알파 배율, w: 가장자리 페이드
    output.coreColorRgb = current.coreColorRgb;

    OutputData[index] = output;
}
