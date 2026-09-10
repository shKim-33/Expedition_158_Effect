#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectMeshCommon.hlsli"

cbuffer cbEffectMaterialPreview : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewParams = { 1.f, 1.f, 0.f, 0.f };                 // x: authored energy scalar, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectMaterialPreviewAlphaParams = { 0.f, 0.f, 0.f, 1.f };            // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectMaterialPreviewMainUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };          // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewMaskUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectMaterialPreviewUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };         // xy: main offset, zw: noise offset
    float4 g_EffectMaterialPreviewMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };     // xy: mask offset, zw: reserved
    float4 g_EffectMaterialPreviewUVModeParams = { 0.f, 0.f, 0.f, 0.f };           // x: main, y: noise, z: mask, w: reserved. 0 Wrap, 1 Stretch, 2 Clamp, 3 Raw
    float4 g_EffectMaterialPreviewUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main U/V policy, zw: noise U/V policy
    float4 g_EffectMaterialPreviewMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialPreviewUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialPreviewSourceParams = { 1.f, 0.f, 0.f, 0.f };           // x: noise source, y: mask source, z: noise invert, w: mask invert
    float4 g_EffectMaterialPreviewAdditiveParams = { 0.f, 0.f, 0.f, 1.f };         // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectMaterialPreviewAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMaterialPreviewAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };          // x: blackNeutral
    float4 g_EffectMaterialPreviewCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };     // x: enabled, y: corePower, z: coreIntensity, w: outerPower
    float4 g_EffectMaterialPreviewCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };  // rgb: coreColor, a: outerIntensity
    float4 g_EffectMaterialPreviewCheckerParams = { 512.f, 512.f, 0.f, 0.f };      // xy: target size, z: time, w: reserved
    float g_EffectMaterialPreviewTime = 0.f;
    int g_OpacitySource = 0;
    float2 g_EffectMaterialPreviewPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

// preview 재질의 UV 스케일과 시간 기반 스크롤을 적용한다.
float2 Apply_MaterialPreviewUV(float2 uv, float4 uvParams, float2 uvOffset)
{
    return uv * uvParams.xy + uvOffset + uvParams.zw * g_EffectMaterialPreviewTime;
}

float Resolve_PreviewAxisUV(float rawValue, float transformedValue, float policy)
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

float2 Build_MaterialPreviewUV(float2 rawUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(rawUV, rotation);
    const float2 transformedUV = Apply_MaterialPreviewUV(rotatedUV, uvParams, uvOffset);
    return float2(
        Resolve_PreviewAxisUV(rotatedUV.x, transformedUV.x, policy.x),
        Resolve_PreviewAxisUV(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float Apply_PreviewAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
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

float4 Sample_PreviewNoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_PreviewAxisAddress(uv, policy));
}

float4 Sample_PreviewMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_PreviewAxisAddress(uv, policy));
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

VS_OUT VS_CHECKER(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.position = float4(In.position.xy, 0.5f, 1.f);
    Out.normal = float3(0.f, 0.f, -1.f);
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

EffectMaterialSurface Resolve_EffectMaterialSurface(PS_IN In)
{
    const float2 mainUVPolicy = g_EffectMaterialPreviewUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMaterialPreviewUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMaterialPreviewMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewMainUVParams,
        g_EffectMaterialPreviewUVOffsetParams.xy,
        mainUVPolicy,
        g_EffectMaterialPreviewUVRotationParams.x);
    const float2 noiseUV = Build_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewNoiseUVParams,
        g_EffectMaterialPreviewUVOffsetParams.zw,
        noiseUVPolicy,
        g_EffectMaterialPreviewUVRotationParams.y);
    const float2 maskUV = Build_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewMaskUVParams,
        g_EffectMaterialPreviewMaskUVOffsetParams.xy,
        maskUVPolicy,
        g_EffectMaterialPreviewUVRotationParams.z);
    const float4 symbol = Sample_PreviewMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMaterialPreviewParams.w && g_EffectMaterialPreviewParams.z > 0.f
        ? Sample_PreviewNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMaterialPreviewAlphaParams.z
        ? Sample_PreviewMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);

    EffectMeshCommonSurfaceInput surfaceInput = (EffectMeshCommonSurfaceInput)0;
    surfaceInput.symbol = symbol;
    surfaceInput.noiseTexel = noiseTexel;
    surfaceInput.maskTexel = maskTexel;
    surfaceInput.instanceColor = float4(1.f, 1.f, 1.f, 1.f);
    surfaceInput.tint = g_Tint;
    surfaceInput.params = g_EffectMaterialPreviewParams;
    surfaceInput.alphaParams = g_EffectMaterialPreviewAlphaParams;
    surfaceInput.sourceParams = g_EffectMaterialPreviewSourceParams;
    surfaceInput.coreEmissiveColor = g_EffectMaterialPreviewCoreEmissiveColor;
    surfaceInput.coreEmissiveParams = g_EffectMaterialPreviewCoreEmissiveParams;
    surfaceInput.opacitySource = g_OpacitySource;
    return EffectMeshCommon_ResolveSurface(surfaceInput);
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    Out.color = Effect_ComposeAlphaBlend(surface, g_EffectMaterialPreviewParams.x);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUVPolicy = g_EffectMaterialPreviewUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMaterialPreviewUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMaterialPreviewMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_MaterialPreviewUV(In.texCoord, g_EffectMaterialPreviewMainUVParams, g_EffectMaterialPreviewUVOffsetParams.xy, mainUVPolicy, g_EffectMaterialPreviewUVRotationParams.x);
    const float2 noiseUV = Build_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewNoiseUVParams,
        g_EffectMaterialPreviewUVOffsetParams.zw,
        noiseUVPolicy,
        g_EffectMaterialPreviewUVRotationParams.y);
    const float2 maskUV = Build_MaterialPreviewUV(
        In.texCoord,
        g_EffectMaterialPreviewMaskUVParams,
        g_EffectMaterialPreviewMaskUVOffsetParams.xy,
        maskUVPolicy,
        g_EffectMaterialPreviewUVRotationParams.z);
    const float4 symbol = Sample_PreviewMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMaterialPreviewParams.w && g_EffectMaterialPreviewParams.z > 0.f
        ? Sample_PreviewNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMaterialPreviewAlphaParams.z
        ? Sample_PreviewMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);

    EffectMeshCommonAdditiveInput additiveInput = (EffectMeshCommonAdditiveInput)0;
    additiveInput.symbol = symbol;
    additiveInput.noiseTexel = noiseTexel;
    additiveInput.maskTexel = maskTexel;
    additiveInput.instanceColor = float4(1.f, 1.f, 1.f, 1.f);
    additiveInput.tint = g_Tint;
    additiveInput.params = g_EffectMaterialPreviewParams;
    additiveInput.alphaParams = g_EffectMaterialPreviewAlphaParams;
    additiveInput.sourceParams = g_EffectMaterialPreviewSourceParams;
    additiveInput.additiveParams = g_EffectMaterialPreviewAdditiveParams;
    additiveInput.additiveEmissiveColor = g_EffectMaterialPreviewAdditiveEmissiveColor;
    additiveInput.additiveConstantColor = g_EffectMaterialPreviewAdditiveConstantColor;
    additiveInput.additiveFlags = g_EffectMaterialPreviewAdditiveFlags;
    additiveInput.coreEmissiveColor = g_EffectMaterialPreviewCoreEmissiveColor;
    additiveInput.coreEmissiveParams = g_EffectMaterialPreviewCoreEmissiveParams;
    additiveInput.opacitySource = g_OpacitySource;
    Out.color = float4(EffectMeshCommon_ResolveAdditiveContribution(additiveInput), 1.f);
    return Out;
}

PS_OUT PS_CHECKER(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float checkerCellSize = 48.f;
    const float2 targetSize = max(g_EffectMaterialPreviewCheckerParams.xy, float2(1.f, 1.f));
    const float2 checkerCoord = In.texCoord * targetSize / checkerCellSize;
    const float2 coarseCell = floor(checkerCoord);
    const float checker = fmod(coarseCell.x + coarseCell.y, 2.f);
    const float2 grid = abs(frac(checkerCoord) - 0.5f);
    const float gridLine = 1.f - smoothstep(0.455f, 0.495f, max(grid.x, grid.y));
    const float vignette = saturate(1.f - length(In.texCoord - 0.5f) * 0.95f);
    const float3 darkColor = float3(0.10f, 0.12f, 0.16f);
    const float3 lightColor = float3(0.34f, 0.39f, 0.46f);
    float3 color = lerp(darkColor, lightColor, checker);

    color = lerp(color, float3(0.72f, 0.78f, 0.86f), gridLine * 0.24f);
    color *= lerp(0.72f, 1.08f, vignette);

    Out.color = float4(color, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    // Plane preview
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_CullNone, DSS_Default, BS_EffectAlphaBlend, VS_MAIN, PS_MAIN) // 0
    // Sphere preview
    PASS_RS_DS_BS_VP(SphereAlphaBlendPass, RS_Default, DSS_Default, BS_EffectAlphaBlend, VS_MAIN, PS_MAIN) // 1
    // Plane additive preview
    PASS_RS_DS_BS_VP(AdditivePass, RS_CullNone, DSS_Default, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE) // 2
    // Sphere additive preview
    PASS_RS_DS_BS_VP(SphereAdditivePass, RS_Default, DSS_Default, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE) // 3
    // Preview-local checker source/background
    PASS_RS_DS_BS_VP(CheckerBackgroundPass, RS_CullNone, DSS_None, BS_Default, VS_CHECKER, PS_CHECKER) // 4
}
