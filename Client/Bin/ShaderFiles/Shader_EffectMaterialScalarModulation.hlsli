static const int EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY = 0;
static const int EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER = 1;
static const int EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH = 2;
static const int EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION = 3;
static const int EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF = 4;
static const int EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY = 5;
static const int EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY = 6;
static const int EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR = 7;
static const int EFFECT_MATERIAL_SCALAR_TARGET_REFRACTION_INTENSITY = 8;
static const int EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_MULTIPLIER = 13;

static const int EFFECT_MATERIAL_SCALAR_TIME_PARTICLE_LIFE = 0;
static const int EFFECT_MATERIAL_SCALAR_TIME_EMITTER_TIME = 1;
static const uint EFFECT_MATERIAL_SCALAR_MAX_CURVE_KEYS = 8u;

// float4에 패킹된 키 데이터에서 지정한 채널 값을 꺼낸다.
float EffectMaterialScalar_ReadComponent(float4 values, uint index)
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

float EffectMaterialScalar_ReadComponent(float4 block0, float4 block1, uint index)
{
    return index < 4u
           ? EffectMaterialScalar_ReadComponent(block0, index)
           : EffectMaterialScalar_ReadComponent(block1, index - 4u);
}

// 모듈레이터 한 개의 커브를 현재 phase 기준으로 평가한다.
float EffectMaterialScalar_EvaluateCurve(uint modulatorIndex, float phase)
{
    float result = 1.f;
    const uint keyCount = min(EFFECT_MATERIAL_SCALAR_MAX_CURVE_KEYS, (uint)g_EffectMaterialScalarModulationMeta[modulatorIndex].w);
    if (keyCount > 0u)
    {
        result = EffectMaterialScalar_ReadComponent(
            g_EffectMaterialScalarModulationKeyValues[modulatorIndex],
            g_EffectMaterialScalarModulationKeyValuesBlock1[modulatorIndex],
            0u
        );
        if (keyCount > 1u)
        {
            const float clampedPhase = saturate(phase);
            [loop]
            for (uint keyIndex = 1u; keyIndex < EFFECT_MATERIAL_SCALAR_MAX_CURVE_KEYS; ++keyIndex)
            {
                if (keyIndex >= keyCount)
                    break;

                const float rightTime = EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialScalarModulationKeyTimes[modulatorIndex],
                    g_EffectMaterialScalarModulationKeyTimesBlock1[modulatorIndex],
                    keyIndex
                );
                if (clampedPhase > rightTime)
                {
                    result = EffectMaterialScalar_ReadComponent(
                        g_EffectMaterialScalarModulationKeyValues[modulatorIndex],
                        g_EffectMaterialScalarModulationKeyValuesBlock1[modulatorIndex],
                        keyIndex
                    );
                    continue;
                }

                const uint leftIndex = keyIndex - 1u;
                const float leftTime = EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialScalarModulationKeyTimes[modulatorIndex],
                    g_EffectMaterialScalarModulationKeyTimesBlock1[modulatorIndex],
                    leftIndex
                );
                const float leftValue = EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialScalarModulationKeyValues[modulatorIndex],
                    g_EffectMaterialScalarModulationKeyValuesBlock1[modulatorIndex],
                    leftIndex
                );
                const float rightValue = EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialScalarModulationKeyValues[modulatorIndex],
                    g_EffectMaterialScalarModulationKeyValuesBlock1[modulatorIndex],
                    keyIndex
                );
                const float width = max(0.0001f, rightTime - leftTime);
                const float ratio = saturate((clampedPhase - leftTime) / width);
                const int mode = (int)EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialScalarModulationKeyModes[modulatorIndex],
                    g_EffectMaterialScalarModulationKeyModesBlock1[modulatorIndex],
                    leftIndex
                );

                if (mode == 0)
                    result = leftValue;
                else if (mode == 2)
                {
                    const float leftLeave = EffectMaterialScalar_ReadComponent(
                                                g_EffectMaterialScalarModulationKeyLeaveTangents[modulatorIndex],
                                                g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1[modulatorIndex],
                                                leftIndex
                                            ) * width;
                    const float rightArrive = EffectMaterialScalar_ReadComponent(
                                                  g_EffectMaterialScalarModulationKeyArriveTangents[modulatorIndex],
                                                  g_EffectMaterialScalarModulationKeyArriveTangentsBlock1[modulatorIndex],
                                                  keyIndex
                                              ) * width;
                    const float t2 = ratio * ratio;
                    const float t3 = t2 * ratio;
                    result = (2.f * t3 - 3.f * t2 + 1.f) * leftValue
                             + (t3 - 2.f * t2 + ratio) * leftLeave
                             + (-2.f * t3 + 3.f * t2) * rightValue
                             + (t3 - t2) * rightArrive;
                }
                else
                    result = lerp(leftValue, rightValue, ratio);
                break;
            }
        }
    }

    return result;
}

// 대상 필드에 연결된 모든 모듈레이터를 곱해 최종 배율을 만든다.
float EffectMaterialScalar_ResolveMultiplier(int targetField, float lifeProgress)
{
    float multiplier = 1.f;
    const uint count = min(8u, (uint)g_EffectMaterialScalarModulationParams.x);

    if (count > 0u)
    {
        [loop]
        for (uint index = 0u; index < 8u; ++index)
        {
            if (index >= count)
                break;

            const float4 meta = g_EffectMaterialScalarModulationMeta[index];
            if (meta.x < 0.5f || (int)meta.y != targetField)
                continue;

            const int timeSource = (int)meta.z;
            const float phase = timeSource == EFFECT_MATERIAL_SCALAR_TIME_EMITTER_TIME
                                ? g_EffectMaterialScalarModulationParams.y
                                : lifeProgress;
            multiplier *= EffectMaterialScalar_EvaluateCurve(index, phase);
        }
    }

    return multiplier;
}

uint EffectMaterial_HashUInt(uint seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return seed;
}

float EffectMaterial_Hash01(uint seed)
{
    return (float)(EffectMaterial_HashUInt(seed) & 0x00FFFFFFu) / 16777215.f;
}

float EffectMaterial_SampleRange(float minValue, float maxValue, uint seed)
{
    const float lower = min(minValue, maxValue);
    const float upper = max(minValue, maxValue);
    return lerp(lower, upper, EffectMaterial_Hash01(seed));
}

uint EffectMaterial_BuildCoreColorSeedBasis(float lifeMax, float4 startColor, float4 endColor)
{
    uint seed = asuint(lifeMax * 1000.f);
    seed ^= asuint(startColor.x * 4096.f) * 16777619u;
    seed ^= asuint(startColor.y * 4096.f) * 2166136261u;
    seed ^= asuint(startColor.z * 4096.f) * 709607u;
    seed ^= asuint(endColor.x * 4096.f) * 9176u;
    seed ^= asuint(endColor.y * 4096.f) * 31337u;
    seed ^= asuint(endColor.z * 4096.f) * 524287u;
    return seed;
}

float EffectMaterialCoreColor_EvaluateComponent(
    uint modulatorIndex,
    float4 keyValues,
    float4 keyValuesBlock1,
    float4 keyArriveTangents,
    float4 keyArriveTangentsBlock1,
    float4 keyLeaveTangents,
    float4 keyLeaveTangentsBlock1,
    float phase,
    float fallbackValue)
{
    float result = fallbackValue;
    const uint keyCount = min(EFFECT_MATERIAL_SCALAR_MAX_CURVE_KEYS, (uint)g_EffectMaterialCoreColorRgbModulationMeta[modulatorIndex].z);
    if (keyCount > 0u)
    {
        result = EffectMaterialScalar_ReadComponent(keyValues, keyValuesBlock1, 0u);
        if (keyCount > 1u)
        {
            const float clampedPhase = saturate(phase);
            [loop]
            for (uint keyIndex = 1u; keyIndex < EFFECT_MATERIAL_SCALAR_MAX_CURVE_KEYS; ++keyIndex)
            {
                if (keyIndex >= keyCount)
                    break;

                const float rightTime = EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialCoreColorRgbModulationKeyTimes[modulatorIndex],
                    g_EffectMaterialCoreColorRgbModulationKeyTimesBlock1[modulatorIndex],
                    keyIndex
                );
                if (clampedPhase > rightTime)
                {
                    result = EffectMaterialScalar_ReadComponent(keyValues, keyValuesBlock1, keyIndex);
                    continue;
                }

                const uint leftIndex = keyIndex - 1u;
                const float leftTime = EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialCoreColorRgbModulationKeyTimes[modulatorIndex],
                    g_EffectMaterialCoreColorRgbModulationKeyTimesBlock1[modulatorIndex],
                    leftIndex
                );
                const float leftValue = EffectMaterialScalar_ReadComponent(keyValues, keyValuesBlock1, leftIndex);
                const float rightValue = EffectMaterialScalar_ReadComponent(keyValues, keyValuesBlock1, keyIndex);
                const float width = max(0.0001f, rightTime - leftTime);
                const float ratio = saturate((clampedPhase - leftTime) / width);
                const int mode = (int)EffectMaterialScalar_ReadComponent(
                    g_EffectMaterialCoreColorRgbModulationKeyModes[modulatorIndex],
                    g_EffectMaterialCoreColorRgbModulationKeyModesBlock1[modulatorIndex],
                    leftIndex
                );

                if (mode == 0)
                    result = leftValue;
                else if (mode == 2)
                {
                    const float leftLeave = EffectMaterialScalar_ReadComponent(keyLeaveTangents, keyLeaveTangentsBlock1, leftIndex) * width;
                    const float rightArrive = EffectMaterialScalar_ReadComponent(keyArriveTangents, keyArriveTangentsBlock1, keyIndex) * width;
                    const float t2 = ratio * ratio;
                    const float t3 = t2 * ratio;
                    result = (2.f * t3 - 3.f * t2 + 1.f) * leftValue
                             + (t3 - 2.f * t2 + ratio) * leftLeave
                             + (-2.f * t3 + 3.f * t2) * rightValue
                             + (t3 - t2) * rightArrive;
                }
                else
                    result = lerp(leftValue, rightValue, ratio);
                break;
            }
        }
    }

    return result;
}

float3 EffectMaterialCoreColor_EvaluateCurve(uint modulatorIndex, float phase, float3 fallbackColor, float3 particleLifeUniformColor)
{
    float3 result = fallbackColor;
    const float4 meta = g_EffectMaterialCoreColorRgbModulationMeta[modulatorIndex];
    if (meta.w >= 0.5f)
        result = particleLifeUniformColor;
    else
    {
        result.x = EffectMaterialCoreColor_EvaluateComponent(
            modulatorIndex,
            g_EffectMaterialCoreColorRgbModulationKeyValuesR[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyValuesRBlock1[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsR[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsRBlock1[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsR[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsRBlock1[modulatorIndex],
            phase,
            fallbackColor.x
        );
        result.y = EffectMaterialCoreColor_EvaluateComponent(
            modulatorIndex,
            g_EffectMaterialCoreColorRgbModulationKeyValuesG[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyValuesGBlock1[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsG[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsGBlock1[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsG[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsGBlock1[modulatorIndex],
            phase,
            fallbackColor.y
        );
        result.z = EffectMaterialCoreColor_EvaluateComponent(
            modulatorIndex,
            g_EffectMaterialCoreColorRgbModulationKeyValuesB[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyValuesBBlock1[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsB[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsBBlock1[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsB[modulatorIndex],
            g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsBBlock1[modulatorIndex],
            phase,
            fallbackColor.z
        );
    }
    return result;
}

float3 EffectMaterialCoreColor_ResolveRgb(float3 baseColor, float lifeProgress, float3 particleLifeUniformColor)
{
    float3 result = baseColor;
    const uint count = min(8u, (uint)g_EffectMaterialCoreColorRgbModulationParams.x);

    [loop]
    for (uint index = 0u; index < 8u; ++index)
    {
        if (index >= count)
            break;

        const float4 meta = g_EffectMaterialCoreColorRgbModulationMeta[index];
        if (meta.x < 0.5f)
            continue;

        const int timeSource = (int)meta.y;
        const float phase = timeSource == EFFECT_MATERIAL_SCALAR_TIME_EMITTER_TIME
                            ? g_EffectMaterialCoreColorRgbModulationParams.y
                            : lifeProgress;
        result = EffectMaterialCoreColor_EvaluateCurve(index, phase, result, particleLifeUniformColor);
    }

    return result;
}
