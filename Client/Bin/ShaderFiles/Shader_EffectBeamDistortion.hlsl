#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectBeamDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectBeamAxisParams = { 0.f, 1.f, 0.f, 0.f };         // x: renderAxis, y: visibleLength, z: tilingDistance, w: smooth start tangent
    float4 g_EffectBeamFallbackAxis = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f };       // x: intensity, y: presence, z: useFlow, w: material time
    float4 g_EffectBeamAlphaParams = { 0.f, 0.f, 0.f, 1.f };        // x: alphaCutoff, y: alphaErosion, z: useMask, w: opacityPower
    float4 g_EffectBeamMainUVParams = { 1.f, 1.f, 0.f, 0.f };       // xy: scale, zw: scroll speed
    float4 g_EffectBeamMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectBeamUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main offset, zw: mask offset
    float4 g_EffectBeamUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f }; // xy: main U/V policy, zw: mask U/V policy
    float4 g_EffectBeamUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectBeamSourceParams = { 0.f, 0.f, 0.f, 0.f };       // x: opacity source, y: mask source, z: mask invert, w: reserved
    float4 g_DistortionScreenSize = { 1.f, 1.f, 0.f, 0.f };
    float2 g_FlowUVPolicyParams = { 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float g_FlowUVTilingMode = 0.f;
    float g_FlowUVRotation = 0.f;
    float g_BeamDistortionPadding = 0.f;

#include "Shader_EffectMaterialModulationPayloadGlobals.hlsli"
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_MainTexture : register(t0);
Texture2D g_MaskTexture : register(t2);
Texture2D g_FlowTexture : register(t3);

static const int BEAM_RENDER_AXIS_CAMERA_UP = 0;
static const int BEAM_RENDER_AXIS_VIEW_UP = 1;
static const int BEAM_RENDER_AXIS_WORLD_UP = 2;
static const float kBeamEdgeFade = 0.04f;

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
    return uv * uvParams.xy + uvOffset + uvParams.zw * g_EffectDistortionParams.w;
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

float4 Sample_BeamMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_BeamAxisAddress(uv, policy));
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

float2 Sample_FlowTexture(float2 uv)
{
    return g_FlowTexture.Sample(
        LinearWrapSampler,
        EffectDistortion_ApplyAxisSeparatedAddress(uv, g_FlowUVPolicyParams)
    ).rg;
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
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

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

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float2 mainUVPolicy = g_EffectBeamUVAxisPolicyParams.xy;
    const float2 maskUVPolicy = g_EffectBeamUVAxisPolicyParams.zw;
    const float2 mainUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamMainUVParams, g_EffectBeamUVOffsetParams.xy, mainUVPolicy, g_EffectBeamUVRotationParams.x);
    const float2 maskUV = Build_BeamMaterialUV(In.texCoord, g_EffectBeamMaskUVParams, g_EffectBeamUVOffsetParams.zw, maskUVPolicy, g_EffectBeamUVRotationParams.y);
    const float4 mainTexel = Sample_BeamMainTexture(mainUV, mainUVPolicy);
    const float selectedOpacity = Effect_SelectOpacity(mainTexel, (int)g_EffectBeamSourceParams.x);
    const float alphaCutoff = saturate(g_EffectBeamAlphaParams.x);
    const float alphaErosion = saturate(g_EffectBeamAlphaParams.y);
    const float opacityPower = max(g_EffectBeamAlphaParams.w, 0.0001f);
    float maskAlpha = 1.f;

    if (0 != g_EffectBeamAlphaParams.z)
    {
        const float4 maskTexel = Sample_BeamMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectBeamSourceParams.y),
            g_EffectBeamSourceParams.z
        );
    }

    const float opacityCoverage = Effect_BuildCoverage(selectedOpacity, opacityPower, maskAlpha, 1.f, 1.f);
    const float materialCoverage = Effect_ApplyAlphaErosion(opacityCoverage, maskAlpha, alphaErosion);
    if (materialCoverage < max(alphaCutoff, 0.01f))
        discard;

    const float sideAlpha =
        smoothstep(0.0f, kBeamEdgeFade, In.edgeCoord.y) *
        smoothstep(0.0f, kBeamEdgeFade, 1.0f - In.edgeCoord.y);
    const float beamCoverage = saturate(materialCoverage * In.color.a * g_Tint.a * saturate(In.segmentParams.w) * sideAlpha);
    const float2 flowUV = Build_FlowUV(In.texCoord);
    const float2 flow = 0 != g_EffectDistortionParams.z
                        ? Sample_FlowTexture(flowUV) * 2.f - 1.f
                        : float2(0.f, 0.f);

    const float lensCoverage = EffectDistortion_ComputeLensCoverage(beamCoverage, 1.f, g_EffectDistortionParams.y);
    const float refractionIntensity =
        g_EffectDistortionParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_REFRACTION_INTENSITY, lifeProgress);
    const float2 offset = EffectDistortion_ComputeScreenOffset(flow, refractionIntensity);
    Out.color = EffectDistortion_PackField(offset, lensCoverage);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(DistortionPass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, GS_MAIN, PS_MAIN)
}
