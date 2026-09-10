#ifndef SHADER_EFFECT_DISTORTION_COMMON_HLSLI
#define SHADER_EFFECT_DISTORTION_COMMON_HLSLI

float EffectDistortion_ComputeShockwavePixelUvWidth(float2 cardUV)
{
    return max(max(fwidth(cardUV.x), fwidth(cardUV.y)), 0.0001f);
}

float EffectDistortion_ComputeShapeCoverage(float4 shapeParams, float2 cardUV, float lifeProgress)
{
    const float mode = shapeParams.x;
    float shapeCoverage = 1.f;

    if (mode >= 0.5f)
    {
        const float radius = max(shapeParams.y, 0.f);
        const float thickness = max(shapeParams.z, 0.f);
        const float softness = max(shapeParams.w, 0.0001f);
        const float distanceFromCenter = distance(cardUV, float2(0.5f, 0.5f));

        if (mode < 1.5f)
            shapeCoverage = 1.f - smoothstep(radius, radius + softness, distanceFromCenter);
        else if (mode < 2.5f)
        {
            const float ringDistance = abs(distanceFromCenter - radius);
            shapeCoverage = 1.f - smoothstep(thickness, thickness + softness, ringDistance);
        }
        else if (mode < 3.5f)
        {
            const float pixelUvWidth = EffectDistortion_ComputeShockwavePixelUvWidth(cardUV);
            const float shockwaveThickness = max(thickness, 0.f) * pixelUvWidth;
            const float shockwaveSoftness = max(softness, 0.0001f) * pixelUvWidth;
            const float shockwaveRadiusMax = max(0.f, 0.5f - shockwaveThickness - shockwaveSoftness);
            const float shockwaveRadius = saturate(lifeProgress) * shockwaveRadiusMax;
            const float ringDistance = abs(distanceFromCenter - shockwaveRadius);
            shapeCoverage = 1.f - smoothstep(shockwaveThickness, shockwaveThickness + shockwaveSoftness, ringDistance);
        }
    }

    return saturate(shapeCoverage);
}

bool EffectDistortion_IsAirSheathMode(float mode)
{
    return mode >= 3.5f && mode < 4.5f;
}

float2 EffectDistortion_ComputeAirSheathField(float widthCoordinate, float2 flow)
{
    const float signedWidthCoordinate = saturate(widthCoordinate) * 2.f - 1.f;
    const float lensProfile =
        4.f * signedWidthCoordinate * (1.f - abs(signedWidthCoordinate));
    const float2 widthGradient = float2(
        ddx(signedWidthCoordinate),
        ddy(signedWidthCoordinate)
    );
    const float widthGradientLengthSquared = dot(widthGradient, widthGradient);
    const float2 widthDirection =
        widthGradientLengthSquared > EPSILON
        ? widthGradient * rsqrt(widthGradientLengthSquared)
        : float2(0.f, 0.f);
    const float flowLengthSquared = dot(flow, flow);
    const float2 boundedFlow =
        flowLengthSquared > 1.f
        ? flow * rsqrt(flowLengthSquared)
        : flow;

    return widthDirection * lensProfile +
           boundedFlow * (0.25f * abs(lensProfile));
}

float2 EffectDistortion_ComputeAirSheathWakeField(float widthCoordinate, float2 trailDirection)
{
    const float centeredWidth = saturate(widthCoordinate) * 2.f - 1.f;
    const float bell = saturate(1.f - centeredWidth * centeredWidth);
    return trailDirection * bell;
}

float EffectDistortion_ComputeLensCoverage(float materialCoverage, float shapeCoverage, float presence)
{
    return saturate(materialCoverage * shapeCoverage * max(presence, 0.f));
}

float EffectDistortion_ComputeLensEdgeAlpha(float lensCoverage)
{
    return saturate(lensCoverage);
}

float2 EffectDistortion_ResolveFieldOffset(float2 accumulatedOffset, float accumulatedLensCoverage)
{
    return accumulatedOffset / max(accumulatedLensCoverage, EPSILON);
}

float2 EffectDistortion_ComputeScreenOffset(float2 flow, float intensity)
{
    return flow * intensity;
}

float EffectDistortion_ApplyAxisPolicy(float rawValue, float transformedValue, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = transformedValue;
    if (mode == 3)
        result = rawValue;
    return result;
}

float EffectDistortion_ApplyAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 3)
        result = value;
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 EffectDistortion_RotateMaterialUV(float2 uv, float rotation)
{
    const int mode = (int)(rotation + 0.5f);
    float2 result = uv;
    if (mode == 1)
        result = float2(1.f - uv.y, uv.x);
    else if (mode == 2)
        result = 1.f - uv;
    else if (mode == 3)
        result = float2(uv.y, 1.f - uv.x);
    return result;
}

float2 EffectDistortion_BuildAxisSeparatedUV(float2 rawUV, float2 scale, float2 offset, float2 scrollSpeed, float materialTime, float2 policy, float rotation)
{
    const float2 rotatedUV = EffectDistortion_RotateMaterialUV(rawUV, rotation);
    const float2 transformedUV = rotatedUV * scale + offset + scrollSpeed * materialTime;
    return float2(
        EffectDistortion_ApplyAxisPolicy(rotatedUV.x, transformedUV.x, policy.x),
        EffectDistortion_ApplyAxisPolicy(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float2 EffectDistortion_ApplyAxisSeparatedAddress(float2 uv, float2 policy)
{
    return float2(
        EffectDistortion_ApplyAxisAddressValue(uv.x, policy.x),
        EffectDistortion_ApplyAxisAddressValue(uv.y, policy.y)
    );
}

float4 EffectDistortion_PackField(float2 offset, float lensCoverage)
{
    const float coverage = saturate(lensCoverage);
    return float4(offset * coverage, coverage, 0.f);
}

#endif
