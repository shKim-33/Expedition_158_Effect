#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectMaterialPreviewDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewAlphaParams = { 0.f, 0.f, 0.f, 1.f };                 // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectMaterialPreviewMainUVParams = { 1.f, 1.f, 0.f, 0.f };                // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewMaskUVParams = { 1.f, 1.f, 0.f, 0.f };                // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewFlowUVParams = { 1.f, 1.f, 0.f, 0.f };                // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };              // xy: main offset, zw: flow offset
    float4 g_EffectMaterialPreviewMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };          // xy: mask offset, zw: reserved
    float4 g_EffectMaterialPreviewMaterialUVPolicyParams = { 1.f, 1.f, 1.f, 1.f };      // xy: main U/V policy, zw: mask U/V policy
    float4 g_EffectMaterialPreviewUVPolicyParams = { 0.f, 0.f, 0.f, 0.f };              // xy: flow U/V policy, zw: reserved
    float4 g_EffectMaterialPreviewUVRotationParams = { 0.f, 0.f, 0.f, 0.f };            // x: main, y: mask, z: flow, w: reserved
    float4 g_EffectMaterialPreviewSourceParams = { 1.f, 0.f, 0.f, 0.f };                // x: opacity power, y: mask source, w: mask invert
    float4 g_EffectMaterialPreviewDistortionParams = { 0.f, 1.f, 0.f, 0.f };            // x: intensity, y: presence, z: useFlow, w: time
    float4 g_EffectMaterialPreviewDistortionShapeParams = { 0.f, 0.45f, 0.10f, 0.15f }; // x: mode, y: radius, z: thickness, w: softness
    float4 g_EffectMaterialPreviewScreenSize = { 512.f, 512.f, 0.f, 0.f };
    float4 g_EffectAirSheathMapParams = { 0.f, 0.f, 1.f, 0.f };
    float2 g_EffectAirSheathMapSpaceInfluence = { 0.f, 0.f };
    int g_OpacitySource = 0;
    float3 g_EffectMaterialPreviewPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_MaskTexture : register(t2);
Texture2D g_FlowTexture : register(t3);
Texture2D g_DistortionSourceTexture : register(t4);

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

float2 Apply_MaterialPreviewUV(float2 uv, float4 uvParams, float2 uvOffset, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(uv, rotation);
    return rotatedUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectMaterialPreviewDistortionParams.w;
}

float Apply_PreviewAxisAddressValue(float value, float mode)
{
    const int resolvedMode = (int)(mode + 0.5f);
    float result = frac(value);
    if (resolvedMode == 1 || resolvedMode == 2)
        result = saturate(value);
    else if (resolvedMode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_PreviewAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_PreviewAxisAddressValue(uv.x, policy.x),
        Apply_PreviewAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_PreviewMainTexture(float2 uv, float2 policy)
{
    return g_Texture.Sample(LinearClampSampler, Apply_PreviewAxisAddress(uv, policy));
}

float4 Sample_PreviewMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_PreviewAxisAddress(uv, policy));
}

float4 Sample_PreviewFlowTexture(float2 uv)
{
    return g_FlowTexture.Sample(
        LinearWrapSampler,
        EffectDistortion_ApplyAxisSeparatedAddress(uv, g_EffectMaterialPreviewUVPolicyParams.xy)
    );
}

struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.position = mul(float4(In.position, 1.f), matWVP);
    Out.normal = normalize(mul(float4(In.normal, 0.f), g_WorldMatrix).xyz);
    Out.texCoord = In.texCoord;

    return Out;
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUVPolicy = g_EffectMaterialPreviewMaterialUVPolicyParams.xy;
    const float2 maskUVPolicy = g_EffectMaterialPreviewMaterialUVPolicyParams.zw;
    const float2 mainUV = Apply_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewMainUVParams,
        g_EffectMaterialPreviewUVOffsetParams.xy,
        g_EffectMaterialPreviewUVRotationParams.x
    );
    const float2 maskUV = Apply_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewMaskUVParams,
        g_EffectMaterialPreviewMaskUVOffsetParams.xy,
        g_EffectMaterialPreviewUVRotationParams.y
    );
    const float2 flowUV = EffectDistortion_BuildAxisSeparatedUV(
        In.texCoord,
        g_EffectMaterialPreviewFlowUVParams.xy,
        g_EffectMaterialPreviewUVOffsetParams.zw,
        g_EffectMaterialPreviewFlowUVParams.zw,
        g_EffectMaterialPreviewDistortionParams.w,
        g_EffectMaterialPreviewUVPolicyParams.xy,
        g_EffectMaterialPreviewUVRotationParams.z
    );
    const float4 symbol = Sample_PreviewMainTexture(mainUV, mainUVPolicy);
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    const float opacityPower = max(g_EffectMaterialPreviewSourceParams.x, 0.0001f);
    const float alphaCutoff = saturate(g_EffectMaterialPreviewAlphaParams.x);
    const float alphaErosion = saturate(g_EffectMaterialPreviewAlphaParams.y);
    float maskAlpha = 1.f;

    if (g_EffectMaterialPreviewAlphaParams.z != 0)
    {
        const float4 maskTexel = Sample_PreviewMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectMaterialPreviewSourceParams.y),
            g_EffectMaterialPreviewSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha, alphaErosion);
    const float materialCoverage = Effect_BuildCoverage(
        selectedOpacity,
        opacityPower,
        maskCoverage,
        1.f,
        g_EffectMaterialPreviewAlphaParams.w
    );
    if (materialCoverage < max(alphaCutoff, 0.01f))
        discard;

    const float previewCoverage = materialCoverage * saturate(g_Tint.a);
    const float4 flowTexel = g_EffectMaterialPreviewDistortionParams.z != 0
                             ? Sample_PreviewFlowTexture(flowUV)
                             : float4(0.5f, 0.5f, 0.5f, 1.f);
    const float2 flow = flowTexel.rg * 2.f - 1.f;
    const bool airSheath = EffectDistortion_IsAirSheathMode(g_EffectMaterialPreviewDistortionShapeParams.x);
    const int interpretation = (int)(g_EffectAirSheathMapParams.x + 0.5f);
    const float mapX = Effect_SelectScalarSource(flowTexel, (int)(g_EffectAirSheathMapParams.y + 0.5f));
    const float mapY = Effect_SelectScalarSource(flowTexel, (int)(g_EffectAirSheathMapParams.z + 0.5f));
    float2 mapField = interpretation == 0 ? float2(mapX, mapY) * 2.f - 1.f : float2(0.f, 0.f);
    if (interpretation == 1)
    {
        uint mapWidth;
        uint mapHeight;
        g_FlowTexture.GetDimensions(mapWidth, mapHeight);
        const float2 texelSize = 1.f / max(float2(mapWidth, mapHeight), float2(1.f, 1.f));
        const int heightSource = (int)(g_EffectAirSheathMapParams.y + 0.5f);
        const float heightXPlus = Effect_SelectScalarSource(Sample_PreviewFlowTexture(flowUV + float2(texelSize.x, 0.f)), heightSource);
        const float heightXMinus = Effect_SelectScalarSource(Sample_PreviewFlowTexture(flowUV - float2(texelSize.x, 0.f)), heightSource);
        const float heightYPlus = Effect_SelectScalarSource(Sample_PreviewFlowTexture(flowUV + float2(0.f, texelSize.y)), heightSource);
        const float heightYMinus = Effect_SelectScalarSource(Sample_PreviewFlowTexture(flowUV - float2(0.f, texelSize.y)), heightSource);
        mapField = float2(heightXPlus - heightXMinus, heightYPlus - heightYMinus);
    }
    if (g_EffectAirSheathMapSpaceInfluence.x != 0.f)
        mapField = float2(mapField.x, mapField.y);
    const float2 baseWake = EffectDistortion_ComputeAirSheathWakeField(In.texCoord.x, float2(1.f, 0.f)) * 24.f;
    const float influence = saturate(g_EffectAirSheathMapSpaceInfluence.y);
    const int composition = (int)(g_EffectAirSheathMapParams.w + 0.5f);
    float2 airSheathField = interpretation == 2
                             ? baseWake * lerp(1.f, mapX, influence)
                             : composition == 0 ? lerp(baseWake, mapField * 24.f, influence) : baseWake + mapField * (24.f * influence);
    const float2 distortionField = airSheath ? airSheathField / max(g_EffectMaterialPreviewScreenSize.xy, float2(1.f, 1.f)) : flow;
    const float2 screenUV = In.position.xy / max(g_EffectMaterialPreviewScreenSize.xy, float2(1.f, 1.f));
    const float shapeCoverage = EffectDistortion_ComputeShapeCoverage(
        g_EffectMaterialPreviewDistortionShapeParams,
        In.texCoord,
        frac(g_EffectMaterialPreviewDistortionParams.w)
    );
    const float lensCoverage = EffectDistortion_ComputeLensCoverage(
        previewCoverage,
        shapeCoverage,
        g_EffectMaterialPreviewDistortionParams.y
    );
    const float2 offset = EffectDistortion_ComputeScreenOffset(distortionField, g_EffectMaterialPreviewDistortionParams.x);
    const float4 distortedColor = g_DistortionSourceTexture.Sample(LinearClampSampler, screenUV + offset);
    const float edgeAlpha = EffectDistortion_ComputeLensEdgeAlpha(lensCoverage);

    Out.color = float4(distortedColor.rgb, edgeAlpha);
    return Out;
}

technique11 DefaultTechnique
{
    // Plane distortion preview
    PASS_RS_DS_BS_VP(DistortionPreviewPass, RS_CullNone, DSS_Default, BS_EffectAlphaBlend, VS_MAIN, PS_MAIN) // 0

    // Sphere distortion preview
    PASS_RS_DS_BS_VP(SphereDistortionPreviewPass, RS_Default, DSS_Default, BS_EffectAlphaBlend, VS_MAIN, PS_MAIN) // 1
}
