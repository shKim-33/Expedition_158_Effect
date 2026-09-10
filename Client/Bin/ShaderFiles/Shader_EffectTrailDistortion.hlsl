#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectTrailDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f }; // x: intensity, y: presence, z: useFlow, w: material time
    float4 g_EffectTrailParams = { 0.f, 1.f, 0.f, 0.f };      // x: shape mode, y: opacityPower
    float4 g_EffectTrailAlphaParams = { 0.f, 0.f, 0.f, 1.f }; // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectTrailMainUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectTrailMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectTrailUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main offset, zw: mask offset
    float4 g_EffectTrailUVPolicyParams = { 1.f, 1.f, 0.f, 0.f };     // x: visible trail length, y: distance tiling
    float4 g_EffectTrailUVAxisPolicyParams = { 1.f, 1.f, 1.f, 1.f }; // xy: main, zw: mask
    float4 g_EffectTrailUVRotationParams = { 0.f, 0.f, 0.f, 0.f };   // x: main, y: mask
    float4 g_EffectTrailSourceParams = { 0.f, 0.f, 0.f, 0.f };       // x: mask source, y: mask invert
    float4 g_DistortionScreenSize = { 1.f, 1.f, 0.f, 0.f };
    float2 g_FlowUVPolicyParams = { 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float4 g_EffectAirSheathMapParams = { 0.f, 0.f, 1.f, 0.f }; // interpretation, X source, Y source, composition
    float2 g_EffectAirSheathMapSpaceInfluence = { 0.f, 0.f };  // space, influence
    float g_FlowUVTilingMode = 0.f;
    float g_FlowUVRotation = 0.f;
    int g_OpacitySource = 0;
    float g_TrailDistortionPadding = 0.f;

#include "Shader_EffectMaterialModulationPayloadGlobals.hlsli"
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_MainTexture : register(t0);
Texture2D g_MaskTexture : register(t2);
Texture2D g_FlowTexture : register(t3);

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

float2 Build_TrailMaterialUV(float2 rawUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(rawUV, rotation);
    const float2 baseUV = float2(
        Resolve_TrailAxisBaseUV(rotatedUV.x, false, policy.x),
        Resolve_TrailAxisBaseUV(rotatedUV.y, true, policy.y)
    );
    return baseUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectDistortionParams.w;
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

float4 Sample_MaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_TrailAxisAddress(uv, policy));
}

float2 Build_FlowUV(float2 rawUV)
{
    return EffectDistortion_BuildAxisSeparatedUV(
        rawUV,
        g_FlowUVScale,
        g_FlowUVOffset,
        g_FlowUVScrollSpeed,
        g_EffectDistortionParams.w,
        g_FlowUVPolicyParams,
        g_FlowUVRotation
    );
}

float4 Sample_FlowTexture(float2 uv)
{
    return g_FlowTexture.Sample(
        LinearWrapSampler,
        EffectDistortion_ApplyAxisSeparatedAddress(uv, g_FlowUVPolicyParams)
    );
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
    float2 trailDirection : TEXCOORD4;
    float2 trailWidthDirection : TEXCOORD5;
    float projectedWidth : TEXCOORD6;
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

    const float4 currentBaseClip = mul(float4(In[0].currentBase, 1.f), viewProj);
    const float4 currentTipClip = mul(float4(In[0].currentTip, 1.f), viewProj);
    const float4 nextBaseClip = mul(float4(In[0].nextBase, 1.f), viewProj);
    const float4 nextTipClip = mul(float4(In[0].nextTip, 1.f), viewProj);
    const float2 currentCenter = (currentBaseClip.xy / max(currentBaseClip.w, EPSILON) + currentTipClip.xy / max(currentTipClip.w, EPSILON)) * 0.25f * g_DistortionScreenSize.xy;
    const float2 nextCenter = (nextBaseClip.xy / max(nextBaseClip.w, EPSILON) + nextTipClip.xy / max(nextTipClip.w, EPSILON)) * 0.25f * g_DistortionScreenSize.xy;
    const float2 segmentVector = nextCenter - currentCenter;
    const float2 widthVector = (currentTipClip.xy / max(currentTipClip.w, EPSILON) - currentBaseClip.xy / max(currentBaseClip.w, EPSILON)) * 0.5f * g_DistortionScreenSize.xy;
    const float segmentLengthSq = dot(segmentVector, segmentVector);
    const float widthLengthSq = dot(widthVector, widthVector);
    Out.trailDirection = currentBaseClip.w > EPSILON && currentTipClip.w > EPSILON && nextBaseClip.w > EPSILON && nextTipClip.w > EPSILON && segmentLengthSq > EPSILON && widthLengthSq > EPSILON ? segmentVector * rsqrt(segmentLengthSq) : float2(0.f, 0.f);
    Out.trailWidthDirection = widthLengthSq > EPSILON ? widthVector * rsqrt(widthLengthSq) : float2(0.f, 0.f);
    Out.projectedWidth = widthLengthSq > EPSILON ? sqrt(widthLengthSq) : 0.f;

    Out.position = currentBaseClip;
    Out.texCoord = In[0].subUVRect.xy;
    Out.edgeCoord = float2(0.f, 0.f);
    OutStream.Append(Out);

    Out.position = currentTipClip;
    Out.texCoord = float2(In[0].subUVRect.z, In[0].subUVRect.y);
    Out.edgeCoord = float2(0.f, 1.f);
    OutStream.Append(Out);

    Out.position = nextBaseClip;
    Out.texCoord = float2(In[0].subUVRect.x, In[0].subUVRect.w);
    Out.edgeCoord = float2(1.f, 0.f);
    OutStream.Append(Out);

    Out.position = nextTipClip;
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
    float2 trailDirection : TEXCOORD4;
    float2 trailWidthDirection : TEXCOORD5;
    float projectedWidth : TEXCOORD6;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float sideFade = max(In.segmentParams.w, 0.0001f);
    const float sideAlpha =
        smoothstep(0.0f, sideFade, In.edgeCoord.y) *
        smoothstep(0.0f, sideFade, 1.0f - In.edgeCoord.y);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float2 mainUVPolicy = g_EffectTrailUVAxisPolicyParams.xy;
    const float2 maskUVPolicy = g_EffectTrailUVAxisPolicyParams.zw;
    const float2 mainUV = Build_TrailMaterialUV(
        In.texCoord,
        g_EffectTrailMainUVParams,
        g_EffectTrailUVOffsetParams.xy,
        mainUVPolicy,
        g_EffectTrailUVRotationParams.x
    );
    const float2 maskUV = Build_TrailMaterialUV(
        In.texCoord,
        g_EffectTrailMaskUVParams,
        g_EffectTrailUVOffsetParams.zw,
        maskUVPolicy,
        g_EffectTrailUVRotationParams.y
    );
    const float4 mainTexel = Sample_MainTexture(mainUV, mainUVPolicy);
    const float selectedOpacity = Effect_SelectOpacity(mainTexel, g_OpacitySource);
    const float resolvedOpacityPower = g_EffectTrailParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedAlphaErosion = g_EffectTrailAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectTrailAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float opacityPower = max(resolvedOpacityPower, 0.0001f);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    float maskAlpha = 1.f;

    if (0 != g_EffectTrailAlphaParams.z)
    {
        const float4 maskTexel = Sample_MaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectTrailSourceParams.x),
            g_EffectTrailSourceParams.y
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha, alphaErosion);
    const float materialCoverage = Effect_BuildCoverage(
        selectedOpacity,
        opacityPower,
        maskCoverage,
        1.f,
        g_EffectTrailAlphaParams.w
    );
    if (materialCoverage < max(alphaCutoff, 0.01f))
        discard;

    const float segmentAlpha = lerp(In.startColor.a, In.endColor.a, lifeProgress);
    const float trailCoverage =
        materialCoverage *
        saturate(segmentAlpha * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);

    const float2 flowUV = Build_FlowUV(In.texCoord);
    const float4 flowTexel = 0 != g_EffectDistortionParams.z ? Sample_FlowTexture(flowUV) : float4(0.5f, 0.5f, 0.5f, 1.f);
    const float2 flow = flowTexel.rg * 2.f - 1.f;
    const bool airSheath = EffectDistortion_IsAirSheathMode(g_EffectTrailParams.x);
    const float widthFade = smoothstep(0.75f, 2.f, In.projectedWidth);
    const float referenceWidth = clamp(In.projectedWidth, 12.f, 96.f);
    const float2 baseWake = EffectDistortion_ComputeAirSheathWakeField(In.edgeCoord.y, In.trailDirection) * referenceWidth * 0.25f;
    float2 mapField = float2(0.f, 0.f);
    const int interpretation = (int)(g_EffectAirSheathMapParams.x + 0.5f);
    const int xSource = (int)(g_EffectAirSheathMapParams.y + 0.5f);
    const int ySource = (int)(g_EffectAirSheathMapParams.z + 0.5f);
    const float mapX = Effect_SelectScalarSource(flowTexel, xSource);
    const float mapY = Effect_SelectScalarSource(flowTexel, ySource);
    if (interpretation == 0)
        mapField = float2(mapX, mapY) * 2.f - 1.f;
    else if (interpretation == 1)
    {
        uint mapWidth;
        uint mapHeight;
        g_FlowTexture.GetDimensions(mapWidth, mapHeight);
        const float2 texelSize = 1.f / max(float2(mapWidth, mapHeight), float2(1.f, 1.f));
        const float heightXPlus = Effect_SelectScalarSource(Sample_FlowTexture(flowUV + float2(texelSize.x, 0.f)), xSource);
        const float heightXMinus = Effect_SelectScalarSource(Sample_FlowTexture(flowUV - float2(texelSize.x, 0.f)), xSource);
        const float heightYPlus = Effect_SelectScalarSource(Sample_FlowTexture(flowUV + float2(0.f, texelSize.y)), xSource);
        const float heightYMinus = Effect_SelectScalarSource(Sample_FlowTexture(flowUV - float2(0.f, texelSize.y)), xSource);
        mapField = float2(heightXPlus - heightXMinus, heightYPlus - heightYMinus);
    }
    const int vectorSpace = (int)(g_EffectAirSheathMapSpaceInfluence.x + 0.5f);
    if (vectorSpace != 0)
        mapField = In.trailDirection * mapField.x + In.trailWidthDirection * mapField.y;
    mapField *= referenceWidth * 0.25f;
    const float mapInfluence = saturate(g_EffectAirSheathMapSpaceInfluence.y);
    const int composition = (int)(g_EffectAirSheathMapParams.w + 0.5f);
    float2 airSheathField = baseWake;
    if (interpretation == 2)
        airSheathField *= lerp(1.f, mapX, mapInfluence);
    else if (composition == 0)
        airSheathField = lerp(baseWake, mapField, mapInfluence);
    else
        airSheathField += mapField * mapInfluence;
    const float2 distortionField = airSheath ? airSheathField * widthFade : flow;

    const float lensCoverage = EffectDistortion_ComputeLensCoverage(trailCoverage, airSheath ? widthFade : 1.f, g_EffectDistortionParams.y);
    const float refractionIntensity =
        g_EffectDistortionParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_REFRACTION_INTENSITY, lifeProgress);
    const float2 offset = airSheath
                          ? distortionField / max(g_DistortionScreenSize.xy, float2(1.f, 1.f)) * refractionIntensity
                          : EffectDistortion_ComputeScreenOffset(distortionField, refractionIntensity);
    Out.color = EffectDistortion_PackField(offset, lensCoverage);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(DistortionPass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, GS_MAIN, PS_MAIN)
}
