#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectMeshCommon.hlsli"

float4x4 g_PreTransformMatrix;

cbuffer cbEffectMeshMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshParams = { 1.f, 1.f, 0.f, 0.f };                 // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectMeshAlphaParams = { 0.f, 0.f, 0.f, 1.f };            // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectMeshMainUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectMeshNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVModeParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main U/V policy, zw: noise U/V policy
    float4 g_EffectMeshMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f }; // xy: mask U/V policy, zw: reserved
    float4 g_EffectMeshUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshSourceParams = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshAdditiveParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectMeshAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };
    float4 g_EffectMeshCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };
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
    float4 g_EffectMeshModelMaterialFlags = { 0.f, 0.f, 0.f, 0.f };      // x: BaseColor, y: Normal, z: Emissive, w: ORM
    int g_OpacitySource = 0;
    float g_EffectMeshMaterialTime = 0.f;
    float2 g_EffectMeshPadding;
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);
Texture2D g_ModelNormalTexture : register(t3);
Texture2D g_ModelEmissiveTexture : register(t4);
Texture2D g_ModelOrmTexture : register(t5);

float Resolve_EffectMeshAxisUV(float rawValue, float transformedValue, float policy)
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

float2 Build_EffectMeshUV(float2 baseUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(baseUV, rotation);
    const float2 transformedUV = rotatedUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectMeshMaterialTime;
    return float2(
        Resolve_EffectMeshAxisUV(rotatedUV.x, transformedUV.x, policy.x),
        Resolve_EffectMeshAxisUV(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float Apply_EffectMeshAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_EffectMeshAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_EffectMeshAxisAddressValue(uv.x, policy.x),
        Apply_EffectMeshAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_EffectMeshMainTexture(float2 uv, float2 policy)
{
    return g_Texture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy));
}

float4 Sample_EffectMeshNoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy));
}

float4 Sample_EffectMeshMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy));
}

float3 Decode_EffectMeshModelNormal(float4 normalTexel)
{
    const float2 normalXY = normalTexel.rg * 2.f - 1.f;
    return normalize(float3(normalXY, sqrt(saturate(1.f - dot(normalXY, normalXY)))));
}

float3 Resolve_EffectMeshModelNormal(float2 uv, float2 policy, float3 normal, float3 tangent, float3 binormal)
{
    if (g_EffectMeshModelMaterialFlags.y <= 0.5f)
        return normalize(normal);

    const float3 tangentNormal = Decode_EffectMeshModelNormal(g_ModelNormalTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)));
    const float3 resolvedTangent = normalize(tangent);
    const float3 resolvedBinormal = normalize(binormal);
    const float3 resolvedNormal = normalize(normal);
    return normalize(
        tangentNormal.x * resolvedTangent +
        tangentNormal.y * resolvedBinormal +
        tangentNormal.z * resolvedNormal
    );
}

float Resolve_EffectMeshModelRoughness(float2 uv, float2 policy)
{
    if (g_EffectMeshModelMaterialFlags.w <= 0.5f)
        return 0.65f;

    return saturate(g_ModelOrmTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)).g);
}

float3 Apply_EffectMeshModelMaterialLighting(float3 color, float3 normal, float2 uv, float2 policy)
{
    if (g_EffectMeshModelMaterialFlags.y <= 0.5f && g_EffectMeshModelMaterialFlags.w <= 0.5f)
        return color;

    const float3 lightDirection = normalize(float3(0.35f, 0.65f, -0.6f));
    const float3 resolvedNormal = normalize(normal);
    const float lighting =
        g_EffectMeshModelMaterialFlags.y > 0.5f
        ? 0.55f + 0.65f * saturate(dot(resolvedNormal, lightDirection))
        : 1.f;

    const float roughness = Resolve_EffectMeshModelRoughness(uv, policy);
    const float visualRoughness = saturate(roughness * 0.75f);
    const float3 viewDirection = normalize(float3(0.f, 0.f, -1.f));
    const float3 halfDirection = normalize(lightDirection + viewDirection);
    const float specularPower = lerp(64.f, 8.f, visualRoughness);
    const float specularStrength = g_EffectMeshModelMaterialFlags.w > 0.5f ? (1.f - visualRoughness) * 0.35f : 0.f;
    const float specular = pow(saturate(dot(resolvedNormal, halfDirection)), specularPower) * specularStrength;
    return color * lighting + float3(specular, specular, specular);
}

float3 Resolve_EffectMeshModelEmissive(float2 uv, float2 policy, float3 modulationColor)
{
    if (g_EffectMeshModelMaterialFlags.z <= 0.5f)
        return float3(0.f, 0.f, 0.f);

    return g_ModelEmissiveTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)).rgb * modulationColor;
}

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD;
    float4 vWorld0 : INST_WORLD0;
    float4 vWorld1 : INST_WORLD1;
    float4 vWorld2 : INST_WORLD2;
    float4 vWorld3 : INST_WORLD3;
    float4 vInstanceColor : INST_COLOR0;
    float2 vLifeTime : INST_LIFE0;
    float4 vCoreColorRgb : INST_CORE_COLOR0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float3 vNormal : TEXCOORD1;
    float4 vInstanceColor : COLOR0;
    float2 vLifeTime : TEXCOORD2;
    float4 vCoreColorRgb : TEXCOORD3;
    float3 vTangent : TEXCOORD4;
    float3 vBinormal : TEXCOORD5;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;
    const float4x4 mWorld = float4x4(In.vWorld0, In.vWorld1, In.vWorld2, In.vWorld3);
    const float4x4 mPreWorld = mul(g_PreTransformMatrix, mWorld);

    Out.vPosition = mul(float4(In.vPosition, 1.f), mul(mPreWorld, mul(g_ViewMatrix, g_ProjMatrix)));
    Out.vTexcoord = In.vTexcoord;
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), mPreWorld)).xyz;
    Out.vTangent = normalize(mul(float4(In.vTangent, 0.f), mPreWorld)).xyz;
    Out.vBinormal = normalize(mul(float4(In.vBinormal, 0.f), mPreWorld)).xyz;
    Out.vInstanceColor = In.vInstanceColor;
    Out.vLifeTime = In.vLifeTime;
    Out.vCoreColorRgb = In.vCoreColorRgb;
    return Out;
}

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMeshSurface(VS_OUT In)
{
    const float lifeProgress = saturate(In.vLifeTime.y / max(In.vLifeTime.x, 0.0001f));
    const float resolvedOpacityPower =
        g_EffectMeshParams.y * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedNoiseStrength =
        g_EffectMeshParams.z * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion =
        g_EffectMeshAlphaParams.y * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff =
        g_EffectMeshAlphaParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity =
        g_EffectMeshCoreEmissiveParams.z * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity =
        g_EffectMeshCoreEmissiveColor.a * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectMeshCoreEmissiveColor.rgb, lifeProgress, In.vCoreColorRgb.rgb);
    const float2 mainUVPolicy = g_EffectMeshUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMeshUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMeshMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, mainUVPolicy, g_EffectMeshUVRotationParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, noiseUVPolicy, g_EffectMeshUVRotationParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, maskUVPolicy, g_EffectMeshUVRotationParams.z);
    const float4 symbol = Sample_EffectMeshMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMeshParams.w && g_EffectMeshParams.z > 0.f
        ? Sample_EffectMeshNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMeshAlphaParams.z
        ? Sample_EffectMeshMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);

    EffectMeshCommonSurfaceInput surfaceInput = (EffectMeshCommonSurfaceInput)0;
    surfaceInput.symbol = symbol;
    surfaceInput.noiseTexel = noiseTexel;
    surfaceInput.maskTexel = maskTexel;
    surfaceInput.instanceColor = In.vInstanceColor;
    surfaceInput.tint = g_Tint;
    surfaceInput.params = float4(g_EffectMeshParams.x, resolvedOpacityPower, resolvedNoiseStrength, g_EffectMeshParams.w);
    surfaceInput.alphaParams = float4(resolvedAlphaCutoff, resolvedAlphaErosion, g_EffectMeshAlphaParams.z, g_EffectMeshAlphaParams.w);
    surfaceInput.sourceParams = g_EffectMeshSourceParams;
    surfaceInput.coreEmissiveColor = float4(resolvedCoreColorRgb * resolvedCoreColorMultiplier, resolvedOuterIntensity);
    surfaceInput.coreEmissiveParams = float4(
        g_EffectMeshCoreEmissiveParams.x,
        g_EffectMeshCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectMeshCoreEmissiveParams.w
    );
    surfaceInput.opacitySource = g_OpacitySource;
    EffectMaterialSurface surface = EffectMeshCommon_ResolveSurface(surfaceInput);
    const float3 modelNormal = Resolve_EffectMeshModelNormal(mainUV, mainUVPolicy, In.vNormal, In.vTangent, In.vBinormal);
    surface.color = Apply_EffectMeshModelMaterialLighting(surface.color, modelNormal, mainUV, mainUVPolicy);
    surface.color += Resolve_EffectMeshModelEmissive(mainUV, mainUVPolicy, In.vInstanceColor.rgb * g_Tint.rgb);
    return surface;
}

PS_OUT PS_ALPHA(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMeshSurface(In);
    const float lifeProgress = saturate(In.vLifeTime.y / max(In.vLifeTime.x, 0.0001f));
    const float resolvedIntensity =
        g_EffectMeshParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = Effect_ComposeAlphaBlend(surface, resolvedIntensity);
    return Out;
}

PS_OUT PS_MASKED(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMeshSurface(In);
    const float lifeProgress = saturate(In.vLifeTime.y / max(In.vLifeTime.x, 0.0001f));
    const float resolvedIntensity =
        g_EffectMeshParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = float4(surface.color * max(resolvedIntensity, 0.f), 1.f);
    return Out;
}

PS_OUT PS_ADDITIVE(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const float lifeProgress = saturate(In.vLifeTime.y / max(In.vLifeTime.x, 0.0001f));
    const float resolvedNoiseStrength =
        g_EffectMeshParams.z * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion =
        g_EffectMeshAlphaParams.y * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff =
        g_EffectMeshAlphaParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity =
        g_EffectMeshCoreEmissiveParams.z * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity =
        g_EffectMeshCoreEmissiveColor.a * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float resolvedIntensity =
        g_EffectMeshParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectMeshCoreEmissiveColor.rgb, lifeProgress, In.vCoreColorRgb.rgb);

    const float2 mainUVPolicy = g_EffectMeshUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMeshUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMeshMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, mainUVPolicy, g_EffectMeshUVRotationParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, noiseUVPolicy, g_EffectMeshUVRotationParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, maskUVPolicy, g_EffectMeshUVRotationParams.z);
    const float4 symbol = Sample_EffectMeshMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMeshParams.w && g_EffectMeshParams.z > 0.f
        ? Sample_EffectMeshNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMeshAlphaParams.z
        ? Sample_EffectMeshMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);

    EffectMeshCommonAdditiveInput additiveInput = (EffectMeshCommonAdditiveInput)0;
    additiveInput.symbol = symbol;
    additiveInput.noiseTexel = noiseTexel;
    additiveInput.maskTexel = maskTexel;
    additiveInput.instanceColor = In.vInstanceColor;
    additiveInput.tint = g_Tint;
    additiveInput.params = float4(resolvedIntensity, g_EffectMeshParams.y, resolvedNoiseStrength, g_EffectMeshParams.w);
    additiveInput.alphaParams = float4(resolvedAlphaCutoff, resolvedAlphaErosion, g_EffectMeshAlphaParams.z, g_EffectMeshAlphaParams.w);
    additiveInput.sourceParams = g_EffectMeshSourceParams;
    additiveInput.additiveParams = g_EffectMeshAdditiveParams;
    additiveInput.additiveEmissiveColor = g_EffectMeshAdditiveEmissiveColor;
    additiveInput.additiveConstantColor = g_EffectMeshAdditiveConstantColor;
    additiveInput.additiveFlags = g_EffectMeshAdditiveFlags;
    additiveInput.coreEmissiveColor = float4(resolvedCoreColorRgb * resolvedCoreColorMultiplier, resolvedOuterIntensity);
    additiveInput.coreEmissiveParams = float4(
        g_EffectMeshCoreEmissiveParams.x,
        g_EffectMeshCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectMeshCoreEmissiveParams.w
    );
    additiveInput.opacitySource = g_OpacitySource;
    float3 additiveColor = EffectMeshCommon_ResolveAdditiveContribution(additiveInput);
    const float3 modelNormal = Resolve_EffectMeshModelNormal(mainUV, mainUVPolicy, In.vNormal, In.vTangent, In.vBinormal);
    additiveColor = Apply_EffectMeshModelMaterialLighting(additiveColor, modelNormal, mainUV, mainUVPolicy);
    additiveColor += Resolve_EffectMeshModelEmissive(mainUV, mainUVPolicy, In.vInstanceColor.rgb * g_Tint.rgb) * max(resolvedIntensity, 0.f);
    Out.color = float4(additiveColor, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_Default, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_ALPHA)          // 0
    PASS_RS_DS_BS_VP(AdditivePass, RS_Default, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE)           // 1
    PASS_RS_DS_BS_VP(AlphaBlendTwoSidedPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_ALPHA) // 2
    PASS_RS_DS_BS_VP(AdditiveTwoSidedPass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE)  // 3
    PASS_RS_DS_BS_VP(MaskedPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_MASKED)                        // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_MASKED)               // 5
}
