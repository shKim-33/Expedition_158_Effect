#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectTrailMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectTrailParams = { 1.f, 1.f, 0.f, 0.f };                 // x: authored energy scalar, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectTrailAlphaParams = { 0.f, 0.f, 0.f, 1.f };            // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectTrailMainUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectTrailNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };          // xy: scale, zw: scroll speed
    float4 g_EffectTrailMaskUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectTrailUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };         // xy: main offset, zw: noise offset
    float4 g_EffectTrailMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };     // xy: mask offset, zw: reserved
    float4 g_EffectTrailUVPolicyParams = { 1.f, 1.f, 0.f, 0.f };         // x: visible trail length, y: noise uv tiling, zw: reserved
    float4 g_EffectTrailUVModeParams = { 0.f, 0.f, 0.f, 0.f };           // x: main, y: noise, z: mask, w: reserved. 0 Wrap, 1 Stretch, 2 Clamp, 3 Raw
    float4 g_EffectTrailUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main U/V policy, zw: noise U/V policy
    float4 g_EffectTrailMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f }; // xy: mask U/V policy, zw: reserved
    float4 g_EffectTrailUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectTrailSourceParams = { 1.f, 0.f, 0.f, 0.f };           // x: noise source, y: mask source, z: noise invert, w: mask invert
    float4 g_EffectTrailAdditiveParams = { 0.f, 0.f, 0.f, 1.f };         // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectTrailAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectTrailAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectTrailAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };          // x: blackNeutral
    float4 g_EffectTrailCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };     // x: enabled, y: corePower, z: coreIntensity, w: outerPower
    float4 g_EffectTrailCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };  // rgb: coreColor, a: outerIntensity
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
    float g_EffectTrailMaterialTime = 0.f;
    int g_OpacitySource = 0;
    float2 g_EffectTrailPadding;
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_MainTexture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

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

float2 Apply_MaterialUV(float2 uv, float4 uvParams, float2 uvOffset)
{
    return uv * uvParams.xy + uvOffset + uvParams.zw * g_EffectTrailMaterialTime;
}

float Resolve_TrailAxisBaseUV(float rawValue, bool lengthAxis, float policy)
{
    const int mode = (int)(policy + 0.5f);
    const float visibleLength = max(g_EffectTrailUVPolicyParams.x, 0.0001f);
    const float uvTiling = max(g_EffectTrailUVPolicyParams.y, 0.0001f);
    float result = rawValue;

    if (mode != 3 && lengthAxis && mode == 1)
        result = saturate(rawValue / visibleLength);
    else if (mode != 3 && lengthAxis)
        result = rawValue * uvTiling;

    return result;
}

float2 Build_TrailSlotBaseUV(float2 rawUV, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(rawUV, rotation);
    return float2(
        Resolve_TrailAxisBaseUV(rotatedUV.x, false, policy.x),
        Resolve_TrailAxisBaseUV(rotatedUV.y, true, policy.y)
    );
}

float Apply_TrailAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_TrailAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_TrailAxisAddressValue(uv.x, policy.x),
        Apply_TrailAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_MainTexture(float2 uv, float2 policy)
{
    return g_MainTexture.Sample(LinearClampSampler, Apply_TrailAxisAddress(uv, policy));
}

float4 Sample_NoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_TrailAxisAddress(uv, policy));
}

float4 Sample_MaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_TrailAxisAddress(uv, policy));
}

struct VS_IN
{
    float3 position : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: 수명 진행률, y: reserved, z: 알파 배율, w: side fade
    float4 coreColorRgb : TEXCOORD6;
};

struct VS_OUT
{
    float3 currentBase : POSITION;
    float3 currentTip : TEXCOORD0;
    float3 nextBase : TEXCOORD1;
    float3 nextTip : TEXCOORD2;
    float2 lifeTime : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD4;
    float4 segmentParams : TEXCOORD5;
    float4 coreColorRgb : TEXCOORD6;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.currentBase = In.InstanceMatrix[0].xyz;
    Out.currentTip = In.InstanceMatrix[1].xyz;
    Out.nextBase = In.InstanceMatrix[2].xyz;
    Out.nextTip = In.InstanceMatrix[3].xyz;
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.segmentParams = In.segmentParams;
    Out.coreColorRgb = In.coreColorRgb;

    return Out;
}

struct GS_IN
{
    float3 currentBase : POSITION;
    float3 currentTip : TEXCOORD0;
    float3 nextBase : TEXCOORD1;
    float3 nextTip : TEXCOORD2;
    float2 lifeTime : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD4;
    float4 segmentParams : TEXCOORD5;
    float4 coreColorRgb : TEXCOORD6;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 segmentParams : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 coreColorRgb : TEXCOORD4;
};

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out;
    Out.lifeTime = In[0].lifeTime;
    Out.startColor = In[0].startColor;
    Out.endColor = In[0].endColor;
    Out.segmentParams = In[0].segmentParams;
    Out.coreColorRgb = In[0].coreColorRgb;

    Out.position = mul(float4(In[0].currentBase, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.xy;
    Out.edgeCoord = float2(0.f, 0.f);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].currentTip, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.z, In[0].subUVRect.y);
    Out.edgeCoord = float2(0.f, 1.f);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].nextBase, 1.f), viewProj);
    Out.texCoord = float2(In[0].subUVRect.x, In[0].subUVRect.w);
    Out.edgeCoord = float2(1.f, 0.f);
    OutStream.Append(Out);

    Out.position = mul(float4(In[0].nextTip, 1.f), viewProj);
    Out.texCoord = In[0].subUVRect.zw;
    Out.edgeCoord = float2(1.f, 1.f);
    OutStream.Append(Out);
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 segmentParams : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 coreColorRgb : TEXCOORD4;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float2 mainUVPolicy = g_EffectTrailUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectTrailUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectTrailMaskUVAxisPolicyParams.xy;
    const float2 mainBaseUV = Build_TrailSlotBaseUV(In.texCoord, mainUVPolicy, g_EffectTrailUVRotationParams.x);
    const float2 noiseBaseUV = Build_TrailSlotBaseUV(In.texCoord, noiseUVPolicy, g_EffectTrailUVRotationParams.y);
    const float2 maskBaseUV = Build_TrailSlotBaseUV(In.texCoord, maskUVPolicy, g_EffectTrailUVRotationParams.z);
    const float2 mainUV = Apply_MaterialUV(mainBaseUV, g_EffectTrailMainUVParams, g_EffectTrailUVOffsetParams.xy);
    const float2 noiseUV = Apply_MaterialUV(noiseBaseUV, g_EffectTrailNoiseUVParams, g_EffectTrailUVOffsetParams.zw);
    const float2 maskUV = Apply_MaterialUV(maskBaseUV, g_EffectTrailMaskUVParams, g_EffectTrailMaskUVOffsetParams.xy);
    const float4 tex = Sample_MainTexture(mainUV, mainUVPolicy);
    const float sideFade = max(In.segmentParams.w, 0.0001f);
    const float sideAlpha =
        smoothstep(0.0f, sideFade, In.edgeCoord.y) *
        smoothstep(0.0f, sideFade, 1.0f - In.edgeCoord.y);
    const float4 segmentColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float resolvedOpacityPower = g_EffectTrailParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedNoiseStrength = g_EffectTrailParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectTrailAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectTrailAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectTrailCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectTrailCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectTrailCoreEmissiveColor.rgb, lifeProgress, In.coreColorRgb.rgb);
    const float opacityPower = max(resolvedOpacityPower, 0.0001f);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectTrailCoreEmissiveParams.x,
        g_EffectTrailCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectTrailCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(resolvedNoiseStrength);
    if (0 != g_EffectTrailParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_NoiseTexture(noiseUV, noiseUVPolicy);
        noiseRaw = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectTrailSourceParams.x),
            g_EffectTrailSourceParams.z
        );
    }

    if (0 != g_EffectTrailAlphaParams.z)
    {
        const float4 maskTexel = Sample_MaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectTrailSourceParams.y),
            g_EffectTrailSourceParams.w
        );
    }

    const float edgeDistance = abs(In.edgeCoord.y - 0.5f) * 2.f;
    const float edgeWeight = smoothstep(0.35f, 1.f, edgeDistance);
    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float edgeBreakup = lerp(1.f, noiseRaw, noiseStrength * edgeWeight);

    float coverage = maskAlpha;

    coverage = Effect_ApplyAlphaErosion(coverage, maskAlpha * edgeBreakup, alphaErosion);

    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        segmentColor.rgb * g_Tint.rgb * tex.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );

    surface.color = baseColor * energyVariation;
    const float shapeCoverage = Effect_BuildCoverage(
        selectedOpacity,
        opacityPower,
        coverage,
        breakupNoise,
        g_EffectTrailAlphaParams.w
    );
    surface.coverage = shapeCoverage * saturate(segmentColor.a * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);

    if (shapeCoverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectTrailParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = Effect_ComposeAlphaBlend(surface, resolvedIntensity);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float2 mainUVPolicy = g_EffectTrailUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectTrailUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectTrailMaskUVAxisPolicyParams.xy;
    const float2 mainBaseUV = Build_TrailSlotBaseUV(In.texCoord, mainUVPolicy, g_EffectTrailUVRotationParams.x);
    const float2 noiseBaseUV = Build_TrailSlotBaseUV(In.texCoord, noiseUVPolicy, g_EffectTrailUVRotationParams.y);
    const float2 maskBaseUV = Build_TrailSlotBaseUV(In.texCoord, maskUVPolicy, g_EffectTrailUVRotationParams.z);
    const float2 mainUV = Apply_MaterialUV(mainBaseUV, g_EffectTrailMainUVParams, g_EffectTrailUVOffsetParams.xy);
    const float2 noiseUV = Apply_MaterialUV(noiseBaseUV, g_EffectTrailNoiseUVParams, g_EffectTrailUVOffsetParams.zw);
    const float2 maskUV = Apply_MaterialUV(maskBaseUV, g_EffectTrailMaskUVParams, g_EffectTrailMaskUVOffsetParams.xy);
    const float4 tex = Sample_MainTexture(mainUV, mainUVPolicy);
    const float sideFade = max(In.segmentParams.w, 0.0001f);
    const float sideAlpha =
        smoothstep(0.0f, sideFade, In.edgeCoord.y) *
        smoothstep(0.0f, sideFade, 1.0f - In.edgeCoord.y);
    const float4 segmentColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float resolvedNoiseStrength = g_EffectTrailParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectTrailAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectTrailAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectTrailCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectTrailCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectTrailCoreEmissiveColor.rgb, lifeProgress, In.coreColorRgb.rgb);
    const float resolvedIntensity = g_EffectTrailParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectTrailCoreEmissiveParams.x,
        g_EffectTrailCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectTrailCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(resolvedNoiseStrength);
    if (0 != g_EffectTrailParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_NoiseTexture(noiseUV, noiseUVPolicy);
        noiseRaw = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectTrailSourceParams.x),
            g_EffectTrailSourceParams.z
        );
    }

    if (0 != g_EffectTrailAlphaParams.z)
    {
        const float4 maskTexel = Sample_MaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectTrailSourceParams.y),
            g_EffectTrailSourceParams.w
        );
    }

    const float edgeDistance = abs(In.edgeCoord.y - 0.5f) * 2.f;
    const float edgeWeight = smoothstep(0.35f, 1.f, edgeDistance);
    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float edgeBreakup = lerp(1.f, noiseRaw, noiseStrength * edgeWeight);
    float coverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * edgeBreakup, alphaErosion);
    const float coverageBase = saturate(coverage * breakupNoise * g_EffectTrailAlphaParams.w);
    const float additiveLifeAlpha = saturate(segmentColor.a * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);
    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const int colorSource = (int)g_EffectTrailAdditiveParams.x;
    const int amountSource = (int)g_EffectTrailAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectTrailAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(tex, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(tex.rgb, colorSource, g_EffectTrailAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        tex.rgb,
        tex.rgb * g_Tint.rgb,
        g_EffectTrailAdditiveEmissiveColor,
        g_EffectTrailAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * segmentColor.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(resolvedIntensity, 0.f) * max(g_EffectTrailAdditiveParams.w, 0.f);
    const float3 contribution = shapedMaterialColor * energyVariation * policyAmount * additiveEnergyScalar * blackGate * additiveLifeAlpha;
    Out.color = float4(contribution, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE) // 1
}
