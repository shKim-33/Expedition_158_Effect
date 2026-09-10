#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectSpriteMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteAlphaParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectSpriteMainUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteUVModeParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteSourceParams = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteAdditiveParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectSpriteAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };
    float4 g_EffectSpriteCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };
    float4 g_EffectInfluenceDebugParams = { 0.f, 0.5f, 2.f, 0.f };
    float4 g_EffectMaterialScalarModulationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialScalarModulationMeta[8];
    float4 g_EffectMaterialScalarModulationKeyTimes[8];
    float4 g_EffectMaterialScalarModulationKeyTimesBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyValues[8];
    float4 g_EffectMaterialScalarModulationKeyValuesBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyArriveTangents[8];
    float4 g_EffectMaterialScalarModulationKeyArriveTangentsBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyLeaveTangents[8];
    float4 g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyModes[8];
    float4 g_EffectMaterialScalarModulationKeyModesBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialCoreColorRgbModulationMeta[8];
    float4 g_EffectMaterialCoreColorRgbModulationUniformMin[8];
    float4 g_EffectMaterialCoreColorRgbModulationUniformMax[8];
    float4 g_EffectMaterialCoreColorRgbModulationUniformSeed[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyTimes[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyTimesBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesR[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesRBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesG[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesGBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesB[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesBBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsR[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsRBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsG[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsGBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsB[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsBBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsR[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsRBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsG[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsGBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsB[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsBBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyModes[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyModesBlock1[8];
    int g_OpacitySource = 0;
    float g_EffectSpriteMaterialTime = 0.f;
    float3 g_EffectSpritePadding;
};

cbuffer cbSourceHistorySpriteTrailRuntime : register(b3)
{
    float4 g_SourceHistorySpriteTrailParams = { 0.f, 0.f, 0.f, 0.f }; // x: screenAlignment, z: flipU, w: flipV
    int g_TextureAxis = 1;
    int g_DirectionalAlignmentMode = 1;
    float g_SpriteRollOffsetRadians = 0.f;
    float g_SourceHistorySpriteTrailPadding;
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

struct VS_IN
{
    float3 position : POSITION;
    float4 centerAndLength : WORLD0;
    float4 tangentAndWidth : WORLD1;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 rotationAndTilt : TEXCOORD2;
    float4 coreColorRgb : TEXCOORD3;
};

struct VS_OUT
{
    float4 centerAndLength : POSITION;
    float4 tangentAndWidth : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float4 rotationAndTilt : TEXCOORD3;
    float4 coreColorRgb : TEXCOORD4;
};

VS_OUT VS_MAIN(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;
    output.centerAndLength = input.centerAndLength;
    output.tangentAndWidth = input.tangentAndWidth;
    output.lifeTime = input.lifeTime;
    output.startColor = input.startColor;
    output.endColor = input.endColor;
    output.subUVRect = input.subUVRect;
    output.rotationAndTilt = input.rotationAndTilt;
    output.coreColorRgb = input.coreColorRgb;
    return output;
}

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float4 coreColorRgb : TEXCOORD3;
};

bool TryNormalize(float3 value, out float3 normalizedValue)
{
    const float lengthSq = dot(value, value);
    normalizedValue = lengthSq > 0.000001f ? value * rsqrt(lengthSq) : float3(0.f, 0.f, 0.f);
    return lengthSq > 0.000001f;
}

float3 NormalizeOrFallback(float3 value, float3 fallbackValue)
{
    float3 normalizedValue = fallbackValue;
    return TryNormalize(value, normalizedValue) ? normalizedValue : fallbackValue;
}

void ResolveCameraPlaneBasis(out float3 rightDir, out float3 upDir)
{
    rightDir = normalize(float3(g_ViewMatrix[0].x, g_ViewMatrix[1].x, g_ViewMatrix[2].x));
    upDir = normalize(float3(g_ViewMatrix[0].y, g_ViewMatrix[1].y, g_ViewMatrix[2].y));
}

float3 ResolveFacingLookDirection(float3 center)
{
    return NormalizeOrFallback(g_CamPosition.xyz - center, float3(0.f, 0.f, 1.f));
}

void ResolveLookAlignedBasis(float3 lookDir, out float3 rightDir, out float3 upDir)
{
    float3 worldUp = float3(0.f, 1.f, 0.f);
    rightDir = cross(lookDir, worldUp);
    if (dot(rightDir, rightDir) < 0.000001f)
        rightDir = cross(lookDir, float3(0.f, 0.f, 1.f));

    rightDir = normalize(rightDir);
    upDir = normalize(cross(rightDir, lookDir));
}

bool TryResolveTextureAxisBasis(float3 axisDir, float3 center, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    float3 axis = float3(0.f, 0.f, 0.f);
    float3 cameraLook = float3(0.f, 0.f, 0.f);
    bool resolved = TryNormalize(axisDir, axis);
    if (resolved)
        resolved = TryNormalize(g_CamPosition.xyz - center, cameraLook);

    if (resolved && g_TextureAxis == 0)
    {
        rightDir = axis;
        resolved = TryNormalize(cross(rightDir, cameraLook), upDir);
    }
    else if (resolved)
    {
        upDir = axis;
        resolved = TryNormalize(cross(cameraLook, upDir), rightDir);
    }

    return resolved;
}

void ResolveWorldUpRectangleBasis(float3 center, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    const float3 worldUp = float3(0.f, 1.f, 0.f);
    float3 cameraLook = float3(0.f, 0.f, 0.f);
    if (!TryNormalize(g_CamPosition.xyz - center, cameraLook))
        return;

    float3 worldRight = float3(0.f, 0.f, 0.f);
    if (!TryNormalize(cross(worldUp, cameraLook), worldRight))
        return;

    rightDir = worldRight;
    upDir = worldUp;
}

void ResolveTrailBasis(float3 center, float3 tangent, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    const int screenAlignment = (int)g_SourceHistorySpriteTrailParams.x;
    const float3 pathAxis = NormalizeOrFallback(tangent, float3(0.f, 1.f, 0.f));
    if (screenAlignment == 0)
        ResolveLookAlignedBasis(ResolveFacingLookDirection(center), rightDir, upDir);
    else if (screenAlignment == 1 || screenAlignment == 7)
        ResolveWorldUpRectangleBasis(center, rightDir, upDir);
    else if (screenAlignment == 4)
    {
        if (g_DirectionalAlignmentMode == 1)
        {
            if (!TryResolveTextureAxisBasis(pathAxis, center, rightDir, upDir))
                ResolveCameraPlaneBasis(rightDir, upDir);
        }
        else
            ResolveLookAlignedBasis(pathAxis, rightDir, upDir);
    }
    else if (screenAlignment != 1 && screenAlignment != 2)
        ResolveLookAlignedBasis(ResolveFacingLookDirection(center), rightDir, upDir);
}

void ApplyRollOffset(inout float3 rightDir, inout float3 upDir, float rotationRadians)
{
    const float rotationCos = cos(rotationRadians);
    const float rotationSin = sin(rotationRadians);
    const float3 baseRight = rightDir;
    const float3 baseUp = upDir;
    rightDir = baseRight * rotationCos + baseUp * rotationSin;
    upDir = -baseRight * rotationSin + baseUp * rotationCos;
}

void ApplySpriteTilt(inout float3 rightDir, inout float3 upDir, float2 tiltDegrees)
{
    const float tiltX = radians(tiltDegrees.x);
    const float tiltY = radians(tiltDegrees.y);
    float3 normalDir = normalize(cross(rightDir, upDir));
    rightDir = normalize(rightDir * cos(tiltX) + normalDir * sin(tiltX));
    normalDir = normalize(cross(rightDir, upDir));
    upDir = normalize(upDir * cos(tiltY) + normalDir * sin(tiltY));
}

void ResolveCardAxes(float3 rightDir, float3 upDir, float length, float width, out float3 textureXDir, out float3 textureYDir)
{
    const bool textureAxisX = g_TextureAxis == 0;
    if (textureAxisX)
    {
        textureXDir = rightDir * (length * 0.5f);
        textureYDir = upDir * (width * 0.5f);
    }
    else
    {
        textureXDir = rightDir * (width * 0.5f);
        textureYDir = upDir * (length * 0.5f);
    }
}

float2 ApplyTrailUVPolicy(float2 uv)
{
    if (g_SourceHistorySpriteTrailParams.z > 0.5f)
        uv.x = 1.f - uv.x;
    if (g_SourceHistorySpriteTrailParams.w > 0.5f)
        uv.y = 1.f - uv.y;
    return uv;
}

[maxvertexcount(4)]
void GS_MAIN(point VS_OUT input[1], inout TriangleStream<GS_OUT> outputStream)
{
    const float3 center = input[0].centerAndLength.xyz;
    const float length = max(input[0].centerAndLength.w, 0.0001f);
    const float width = max(input[0].tangentAndWidth.w, 0.0001f);
    const float3 tangent = input[0].tangentAndWidth.xyz;
    const int screenAlignment = (int)g_SourceHistorySpriteTrailParams.x;

    float3 rightDir = float3(1.f, 0.f, 0.f);
    float3 upDir = float3(0.f, 1.f, 0.f);
    ResolveTrailBasis(center, tangent, rightDir, upDir);

    ApplyRollOffset(rightDir, upDir, input[0].rotationAndTilt.x + g_SpriteRollOffsetRadians);
    ApplySpriteTilt(rightDir, upDir, input[0].rotationAndTilt.yz);
    float3 textureXDir = float3(0.f, 0.f, 0.f);
    float3 textureYDir = float3(0.f, 0.f, 0.f);
    ResolveCardAxes(rightDir, upDir, length, width, textureXDir, textureYDir);
    if (screenAlignment == 2)
    {
        const float squareExtent = max(length, width) * 0.5f;
        textureXDir = rightDir * squareExtent;
        textureYDir = upDir * squareExtent;
    }

    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);
    GS_OUT output[4] = (GS_OUT[4])0;
    const float2 baseUV[4] = {
        float2(0.f, 0.f),
        float2(1.f, 0.f),
        float2(0.f, 1.f),
        float2(1.f, 1.f)
    };
    const float3 positions[4] = {
        center - textureXDir + textureYDir,
        center + textureXDir + textureYDir,
        center - textureXDir - textureYDir,
        center + textureXDir - textureYDir
    };

    [unroll]
    for (int index = 0; index < 4; ++index)
    {
        output[index].position = mul(float4(positions[index], 1.f), viewProj);
        output[index].texCoord = ApplyTrailUVPolicy(baseUV[index]);
        output[index].lifeTime = input[0].lifeTime;
        output[index].startColor = input[0].startColor;
        output[index].endColor = input[0].endColor;
        output[index].subUVRect = input[0].subUVRect;
        output[index].coreColorRgb = input[0].coreColorRgb;
        outputStream.Append(output[index]);
    }
}

float Resolve_EffectSpriteAxisUV(float rawValue, float transformedValue, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = transformedValue;
    if (mode == 3)
        result = rawValue;
    return result;
}

float2 Rotate_MaterialUV(float2 uv, float rotation)
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

float2 Build_EffectSpriteUV(float2 baseUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(baseUV, rotation);
    const float2 transformedUV = rotatedUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectSpriteMaterialTime;
    return float2(
        Resolve_EffectSpriteAxisUV(rotatedUV.x, transformedUV.x, policy.x),
        Resolve_EffectSpriteAxisUV(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float Apply_EffectSpriteAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_EffectSpriteAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_EffectSpriteAxisAddressValue(uv.x, policy.x),
        Apply_EffectSpriteAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_EffectSpriteMainTexture(float2 uv, float2 policy)
{
    return g_Texture.Sample(LinearClampSampler, Apply_EffectSpriteAxisAddress(uv, policy));
}

float4 Sample_EffectSpriteNoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_EffectSpriteAxisAddress(uv, policy));
}

float4 Sample_EffectSpriteMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_EffectSpriteAxisAddress(uv, policy));
}

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMaterialSurface(GS_OUT input)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;
    const float lifeProgress = saturate(input.lifeTime.y / max(input.lifeTime.x, 0.0001f));
    const float4 particleColor = lerp(input.startColor, input.endColor, lifeProgress);
    const float2 sampleUV = lerp(input.subUVRect.xy, input.subUVRect.zw, input.texCoord);
    const float2 mainUVPolicy = g_EffectSpriteUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectSpriteUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectSpriteMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMainUVParams, g_EffectSpriteUVOffsetParams.xy, mainUVPolicy, g_EffectSpriteUVRotationParams.x);
    const float4 symbol = Sample_EffectSpriteMainTexture(mainUV, mainUVPolicy);
    const float resolvedOpacityPower = g_EffectSpriteParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedNoiseStrength = 0 != g_EffectSpriteParams.w && g_EffectSpriteParams.z > 0.f
                                        ? g_EffectSpriteParams.z * EffectMaterialScalar_ResolveMultiplier(
                                              EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH,
                                              lifeProgress
                                          )
                                        : 0.f;
    const float resolvedAlphaErosion = g_EffectSpriteAlphaParams.y != 0.f
                                       ? g_EffectSpriteAlphaParams.y * EffectMaterialScalar_ResolveMultiplier(
                                             EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION,
                                             lifeProgress
                                         )
                                       : 0.f;
    const float resolvedAlphaCutoff = g_EffectSpriteAlphaParams.x != 0.f
                                      ? g_EffectSpriteAlphaParams.x * EffectMaterialScalar_ResolveMultiplier(
                                            EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF,
                                            lifeProgress
                                        )
                                      : 0.f;
    const float resolvedCoreIntensity = g_EffectSpriteCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectSpriteCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectSpriteCoreEmissiveColor.rgb, lifeProgress, input.coreColorRgb.rgb);
    const float opacityPower = max(resolvedOpacityPower, 0.0001f);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectSpriteCoreEmissiveParams.x,
        g_EffectSpriteCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectSpriteCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && resolvedNoiseStrength > 0.f)
    {
        const float2 noiseUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteNoiseUVParams, g_EffectSpriteUVOffsetParams.zw, noiseUVPolicy, g_EffectSpriteUVRotationParams.y);
        const float4 noiseTexel = Sample_EffectSpriteNoiseTexture(noiseUV, noiseUVPolicy);
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectSpriteSourceParams.x),
            g_EffectSpriteSourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(resolvedNoiseStrength));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
    {
        const float2 maskUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMaskUVParams, g_EffectSpriteMaskUVOffsetParams.xy, maskUVPolicy, g_EffectSpriteUVRotationParams.z);
        const float4 maskTexel = Sample_EffectSpriteMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectSpriteSourceParams.y),
            g_EffectSpriteSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float shapeCoverage = Effect_BuildCoverage(selectedOpacity, opacityPower, maskCoverage, noiseFactor, g_EffectSpriteAlphaParams.w);
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        symbol.rgb * particleColor.rgb * g_Tint.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    surface.color = baseColor * noiseFactor;
    surface.coverage = shapeCoverage * saturate(particleColor.a * g_Tint.a);

    if (shapeCoverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(GS_OUT input)
{
    PS_OUT output = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(input);
    const float lifeProgress = saturate(input.lifeTime.y / max(input.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectSpriteParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    output.color = Effect_ComposeAlphaBlend(surface, resolvedIntensity);
    return output;
}

PS_OUT PS_MASKED(GS_OUT input)
{
    PS_OUT output = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(input);
    const float lifeProgress = saturate(input.lifeTime.y / max(input.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectSpriteParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    output.color = float4(surface.color * max(resolvedIntensity, 0.f), 1.f);
    return output;
}

PS_OUT PS_ADDITIVE(GS_OUT input)
{
    PS_OUT output = (PS_OUT)0;
    const float lifeProgress = saturate(input.lifeTime.y / max(input.lifeTime.x, 0.0001f));
    const float4 particleColor = lerp(input.startColor, input.endColor, lifeProgress);
    const float2 sampleUV = lerp(input.subUVRect.xy, input.subUVRect.zw, input.texCoord);
    const float2 mainUVPolicy = g_EffectSpriteUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectSpriteUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectSpriteMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMainUVParams, g_EffectSpriteUVOffsetParams.xy, mainUVPolicy, g_EffectSpriteUVRotationParams.x);
    const float2 noiseUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteNoiseUVParams, g_EffectSpriteUVOffsetParams.zw, noiseUVPolicy, g_EffectSpriteUVRotationParams.y);
    const float2 maskUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMaskUVParams, g_EffectSpriteMaskUVOffsetParams.xy, maskUVPolicy, g_EffectSpriteUVRotationParams.z);
    const float4 symbol = Sample_EffectSpriteMainTexture(mainUV, mainUVPolicy);
    const float resolvedNoiseStrength = g_EffectSpriteParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectSpriteAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectSpriteAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectSpriteCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectSpriteCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectSpriteCoreEmissiveColor.rgb, lifeProgress, input.coreColorRgb.rgb);
    const float resolvedIntensity = g_EffectSpriteParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectSpriteCoreEmissiveParams.x,
        g_EffectSpriteCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectSpriteCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_EffectSpriteNoiseTexture(noiseUV, noiseUVPolicy);
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectSpriteSourceParams.x),
            g_EffectSpriteSourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(resolvedNoiseStrength));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
    {
        const float4 maskTexel = Sample_EffectSpriteMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectSpriteSourceParams.y),
            g_EffectSpriteSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverageBase = saturate(maskCoverage * noiseFactor * g_EffectSpriteAlphaParams.w);
    const float additiveLifeAlpha = saturate(particleColor.a * g_Tint.a);
    const int colorSource = (int)g_EffectSpriteAdditiveParams.x;
    const int amountSource = (int)g_EffectSpriteAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectSpriteAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(symbol, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(symbol.rgb, colorSource, g_EffectSpriteAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        symbol.rgb,
        symbol.rgb * g_Tint.rgb,
        g_EffectSpriteAdditiveEmissiveColor,
        g_EffectSpriteAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * particleColor.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(resolvedIntensity, 0.f) * max(g_EffectSpriteAdditiveParams.w, 0.f);
    output.color = float4(shapedMaterialColor * noiseFactor * policyAmount * additiveEnergyScalar * blackGate * additiveLifeAlpha, 1.f);
    return output;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN)
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE)
    PASS_RS_DS_BS_VGP(MaskedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, GS_MAIN, PS_MASKED)
}
