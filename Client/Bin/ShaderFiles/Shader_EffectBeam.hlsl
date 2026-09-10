#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectBeamMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectBeamParams = { 1.f, 1.f, 0.f, 0.f };                 // x: authored energy scalar, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectBeamAlphaParams = { 0.f, 0.f, 0.f, 1.f };            // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectBeamMainUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectBeamNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectBeamMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectBeamUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };         // xy: main offset, zw: noise offset
    float4 g_EffectBeamMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectBeamUVModeParams = { 0.f, 0.f, 0.f, 0.f };           // x: main, y: noise, z: mask, w: reserved
    float4 g_EffectBeamUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main U/V policy, zw: noise U/V policy
    float4 g_EffectBeamMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f }; // xy: mask U/V policy, zw: reserved
    float4 g_EffectBeamUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectBeamSourceParams = { 1.f, 0.f, 0.f, 0.f };           // x: noise source, y: mask source, z: noise invert, w: mask invert
    float4 g_EffectBeamAdditiveParams = { 0.f, 0.f, 0.f, 1.f };         // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectBeamAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectBeamAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectBeamAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectBeamCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };     // x: enabled, y: corePower, z: coreIntensity, w: outerPower
    float4 g_EffectBeamCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };  // rgb: coreColor, a: outerIntensity
    float4 g_EffectBeamAxisParams = { 0.f, 1.f, 0.f, 0.f };             // x: renderAxis, y: visibleLength, z: tilingDistance, w: smooth start tangent
    float4 g_EffectBeamFallbackAxis = { 1.f, 0.f, 0.f, 0.f };
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
    float g_EffectBeamMaterialTime = 0.f;
    float2 g_EffectBeamPadding;
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_MainTexture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

static const int BEAM_RENDER_AXIS_CAMERA_UP = 0;
static const int BEAM_RENDER_AXIS_VIEW_UP = 1;
static const int BEAM_RENDER_AXIS_WORLD_UP = 2;
static const float kBeamEdgeFade = 0.04f;

float Resolve_BeamAxisUV(float rawValue, float transformedValue, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = transformedValue;
    if (mode == 3)
        result = rawValue;
    return result;
}

float2 Apply_BeamMaterialUV(float2 uv, float4 uvParams, float2 uvOffset)
{
    return uv * uvParams.xy + uvOffset + uvParams.zw * g_EffectBeamMaterialTime;
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

float2 Build_BeamMaterialUV(float2 rawUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(rawUV, rotation);
    const float2 transformedUV = Apply_BeamMaterialUV(rotatedUV, uvParams, uvOffset);
    return float2(
        Resolve_BeamAxisUV(rotatedUV.x, transformedUV.x, policy.x),
        Resolve_BeamAxisUV(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float Apply_BeamAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_BeamAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_BeamAxisAddressValue(uv.x, policy.x),
        Apply_BeamAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_BeamMainTexture(float2 uv, float2 policy)
{
    return g_MainTexture.Sample(LinearClampSampler, Apply_BeamAxisAddress(uv, policy));
}

float4 Sample_BeamNoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_BeamAxisAddress(uv, policy));
}

float4 Sample_BeamMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_BeamAxisAddress(uv, policy));
}

struct VS_IN
{
    float3 position : POSITION;
    float4 previousPosition : WORLD0;
    float4 currentPosition : WORLD1;
    float4 nextPosition : WORLD2;
    float4 nextNextPosition : WORLD3;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: life progress, y: current width, z: next width, w: alpha scale
    float4 coreColorRgb : TEXCOORD6;
};

struct VS_OUT
{
    float4 previousPosition : POSITION;
    float4 currentPosition : TEXCOORD0;
    float4 nextPosition : TEXCOORD1;
    float4 nextNextPosition : TEXCOORD2;
    float2 lifeTime : TEXCOORD3;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD4;
    float4 segmentParams : TEXCOORD5;
    float4 coreColorRgb : TEXCOORD6;
};

struct GS_IN
{
    float4 previousPosition : POSITION;
    float4 currentPosition : TEXCOORD0;
    float4 nextPosition : TEXCOORD1;
    float4 nextNextPosition : TEXCOORD2;
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
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
    float4 coreColorRgb : TEXCOORD4;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
    float4 coreColorRgb : TEXCOORD4;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

float3 NormalizeOrFallback(float3 value, float3 fallbackAxis)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 0.000001f ? value * rsqrt(lengthSq) : fallbackAxis;
}

float3 Resolve_ViewUpAxis()
{
    return NormalizeOrFallback(g_ViewMatrixInverse[1].xyz, float3(0.f, 1.f, 0.f));
}

float3 Resolve_BeamTangent(
    float3 previousPosition,
    float3 currentPosition,
    float3 nextPosition,
    bool hasPreviousSample,
    bool hasNextSample)
{
    const float3 fallbackAxis = float3(0.f, 0.f, 1.f);
    float3 tangent = fallbackAxis;

    if (hasNextSample)
    {
        const float3 forward = NormalizeOrFallback(nextPosition - currentPosition, fallbackAxis);
        tangent = forward;
        if (hasPreviousSample && g_EffectBeamAxisParams.w >= 0.5f)
        {
            const float3 backward = NormalizeOrFallback(currentPosition - previousPosition, forward);
            tangent = NormalizeOrFallback(backward + forward, forward);
        }
    }
    else if (hasPreviousSample)
        tangent = NormalizeOrFallback(currentPosition - previousPosition, fallbackAxis);

    return tangent;
}

float3 Resolve_BeamSide(float3 currentPosition, float3 tangent)
{
    const float3 fallbackAxis = NormalizeOrFallback(g_EffectBeamFallbackAxis.xyz, float3(1.f, 0.f, 0.f));
    const int renderAxis = (int)g_EffectBeamAxisParams.x;

    float3 axis = NormalizeOrFallback(g_CamPosition.xyz - currentPosition, float3(0.f, 0.f, -1.f));
    if (renderAxis == BEAM_RENDER_AXIS_VIEW_UP)
        axis = Resolve_ViewUpAxis();
    else if (renderAxis == BEAM_RENDER_AXIS_WORLD_UP)
        axis = float3(0.f, 1.f, 0.f);

    float3 side = NormalizeOrFallback(cross(axis, tangent), fallbackAxis);
    if (abs(dot(side, tangent)) > 0.98f)
        side = NormalizeOrFallback(cross(Resolve_ViewUpAxis(), tangent), fallbackAxis);

    return side;
}

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.previousPosition = In.previousPosition;
    Out.currentPosition = In.currentPosition;
    Out.nextPosition = In.nextPosition;
    Out.nextNextPosition = In.nextNextPosition;
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.segmentParams = In.segmentParams;
    Out.coreColorRgb = In.coreColorRgb;

    return Out;
}

GS_OUT Build_BeamVertex(
    float3 worldPosition,
    float2 texCoord,
    float2 edgeCoord,
    GS_IN input)
{
    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out = (GS_OUT)0;
    Out.position = mul(float4(worldPosition, 1.f), viewProj);
    Out.texCoord = texCoord;
    Out.lifeTime = input.lifeTime;
    Out.edgeCoord = edgeCoord;
    Out.color = lerp(input.startColor, input.endColor, saturate(input.segmentParams.x));
    Out.segmentParams = input.segmentParams;
    Out.coreColorRgb = input.coreColorRgb;
    return Out;
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 previousPosition = In[0].previousPosition.xyz;
    const float3 currentPosition = In[0].currentPosition.xyz;
    const float3 nextPosition = In[0].nextPosition.xyz;
    const float3 nextNextPosition = In[0].nextNextPosition.xyz;
    const bool hasPreviousSample = In[0].previousPosition.w > 0.5f;
    const bool hasNextNextSample = In[0].nextNextPosition.w > 0.5f;
    const float3 currentTangent = Resolve_BeamTangent(
        previousPosition,
        currentPosition,
        nextPosition,
        hasPreviousSample,
        true
    );
    const float3 nextTangent = Resolve_BeamTangent(
        currentPosition,
        nextPosition,
        nextNextPosition,
        true,
        hasNextNextSample
    );
    const float3 currentSide = Resolve_BeamSide(currentPosition, currentTangent);
    const float3 nextSide = Resolve_BeamSide(nextPosition, nextTangent);
    const float currentHalfWidth = max(In[0].segmentParams.y, 0.001f) * 0.5f;
    const float nextHalfWidth = max(In[0].segmentParams.z, 0.001f) * 0.5f;

    const float3 currentA = currentPosition - currentSide * currentHalfWidth;
    const float3 currentB = currentPosition + currentSide * currentHalfWidth;
    const float3 nextA = nextPosition - nextSide * nextHalfWidth;
    const float3 nextB = nextPosition + nextSide * nextHalfWidth;

    OutStream.Append(Build_BeamVertex(currentA, In[0].subUVRect.xy, float2(0.f, 0.f), In[0]));
    OutStream.Append(Build_BeamVertex(currentB, float2(In[0].subUVRect.z, In[0].subUVRect.y), float2(0.f, 1.f), In[0]));
    OutStream.Append(Build_BeamVertex(nextA, float2(In[0].subUVRect.x, In[0].subUVRect.w), float2(1.f, 0.f), In[0]));
    OutStream.Append(Build_BeamVertex(nextB, In[0].subUVRect.zw, float2(1.f, 1.f), In[0]));
}

EffectMaterialSurface Resolve_EffectBeamMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float2 mainUVPolicy = g_EffectBeamUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectBeamUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectBeamMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamMainUVParams, g_EffectBeamUVOffsetParams.xy, mainUVPolicy, g_EffectBeamUVRotationParams.x);
    const float2 noiseUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamNoiseUVParams, g_EffectBeamUVOffsetParams.zw, noiseUVPolicy, g_EffectBeamUVRotationParams.y);
    const float2 maskUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamMaskUVParams, g_EffectBeamMaskUVOffsetParams.xy, maskUVPolicy, g_EffectBeamUVRotationParams.z);
    const float4 tex = Sample_BeamMainTexture(mainUV, mainUVPolicy);
    const float sideAlpha =
        smoothstep(0.0f, kBeamEdgeFade, In.edgeCoord.y) *
        smoothstep(0.0f, kBeamEdgeFade, 1.0f - In.edgeCoord.y);
    const float resolvedOpacityPower = g_EffectBeamParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedNoiseStrength = g_EffectBeamParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectBeamAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectBeamAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectBeamCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectBeamCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectBeamCoreEmissiveColor.rgb, lifeProgress, In.coreColorRgb.rgb);
    const float opacityPower = max(resolvedOpacityPower, 0.0001f);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectBeamCoreEmissiveParams.x,
        g_EffectBeamCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectBeamCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(resolvedNoiseStrength);
    if (0 != g_EffectBeamParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_BeamNoiseTexture(noiseUV, noiseUVPolicy);
        noiseRaw = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectBeamSourceParams.x),
            g_EffectBeamSourceParams.z
        );
    }

    if (0 != g_EffectBeamAlphaParams.z)
    {
        const float4 maskTexel = Sample_BeamMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectBeamSourceParams.y),
            g_EffectBeamSourceParams.w
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
        In.color.rgb * g_Tint.rgb * tex.rgb,
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
        g_EffectBeamAlphaParams.w
    );
    surface.coverage = shapeCoverage * saturate(In.color.a * g_Tint.a * saturate(In.segmentParams.w) * sideAlpha);

    if (shapeCoverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectBeamMaterialSurface(In);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectBeamParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = Effect_ComposeAlphaBlend(surface, resolvedIntensity);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float2 mainUVPolicy = g_EffectBeamUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectBeamUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectBeamMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamMainUVParams, g_EffectBeamUVOffsetParams.xy, mainUVPolicy, g_EffectBeamUVRotationParams.x);
    const float2 noiseUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamNoiseUVParams, g_EffectBeamUVOffsetParams.zw, noiseUVPolicy, g_EffectBeamUVRotationParams.y);
    const float2 maskUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamMaskUVParams, g_EffectBeamMaskUVOffsetParams.xy, maskUVPolicy, g_EffectBeamUVRotationParams.z);
    const float4 tex = Sample_BeamMainTexture(mainUV, mainUVPolicy);
    const float sideAlpha =
        smoothstep(0.0f, kBeamEdgeFade, In.edgeCoord.y) *
        smoothstep(0.0f, kBeamEdgeFade, 1.0f - In.edgeCoord.y);
    const float resolvedNoiseStrength = g_EffectBeamParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectBeamAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectBeamAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectBeamCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectBeamCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectBeamCoreEmissiveColor.rgb, lifeProgress, In.coreColorRgb.rgb);
    const float resolvedIntensity = g_EffectBeamParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectBeamCoreEmissiveParams.x,
        g_EffectBeamCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectBeamCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(resolvedNoiseStrength);
    if (0 != g_EffectBeamParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_BeamNoiseTexture(noiseUV, noiseUVPolicy);
        noiseRaw = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectBeamSourceParams.x),
            g_EffectBeamSourceParams.z
        );
    }

    if (0 != g_EffectBeamAlphaParams.z)
    {
        const float4 maskTexel = Sample_BeamMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectBeamSourceParams.y),
            g_EffectBeamSourceParams.w
        );
    }

    const float edgeDistance = abs(In.edgeCoord.y - 0.5f) * 2.f;
    const float edgeWeight = smoothstep(0.35f, 1.f, edgeDistance);
    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float edgeBreakup = lerp(1.f, noiseRaw, noiseStrength * edgeWeight);
    float coverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * edgeBreakup, alphaErosion);
    const float coverageBase = saturate(coverage * breakupNoise * g_EffectBeamAlphaParams.w);
    const float additiveLifeAlpha = saturate(In.color.a * g_Tint.a * saturate(In.segmentParams.w) * sideAlpha);
    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const int colorSource = (int)g_EffectBeamAdditiveParams.x;
    const int amountSource = (int)g_EffectBeamAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectBeamAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(tex, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(tex.rgb, colorSource, g_EffectBeamAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        tex.rgb,
        tex.rgb * g_Tint.rgb,
        g_EffectBeamAdditiveEmissiveColor,
        g_EffectBeamAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * In.color.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(resolvedIntensity, 0.f) * max(g_EffectBeamAdditiveParams.w, 0.f);
    const float3 contribution = shapedMaterialColor * energyVariation * policyAmount * additiveEnergyScalar * blackGate * additiveLifeAlpha;
    Out.color = float4(contribution, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN)
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE)
}
