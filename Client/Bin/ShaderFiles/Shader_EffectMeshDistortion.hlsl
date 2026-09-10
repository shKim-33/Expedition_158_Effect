#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

float4x4 g_PreTransformMatrix;

cbuffer cbEffectMeshDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f }; // x:intensity, y:presence, z:useFlow, w:material time
    float4 g_DistortionScreenSize = { 1.f, 1.f, 0.f, 0.f };
    float2 g_FlowUVPolicyParams = { 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float g_FlowUVTilingMode = 0.f;
    float g_FlowUVRotation = 0.f;
    float g_EffectMeshDistortionPadding = 0.f;

#include "Shader_EffectMaterialModulationPayloadGlobals.hlsli"
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_FlowTexture : register(t3);

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
    float4 vInstanceColor : COLOR0;
    float2 vLifeTime : TEXCOORD1;
};

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

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;
    const float4x4 mWorld = float4x4(In.vWorld0, In.vWorld1, In.vWorld2, In.vWorld3);
    const float4x4 mPreWorld = mul(g_PreTransformMatrix, mWorld);

    Out.vPosition = mul(float4(In.vPosition, 1.f), mul(mPreWorld, mul(g_ViewMatrix, g_ProjMatrix)));
    Out.vTexcoord = In.vTexcoord;
    Out.vInstanceColor = In.vInstanceColor;
    Out.vLifeTime = In.vLifeTime;
    return Out;
}

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

PS_OUT PS_MAIN(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const float lifeProgress = saturate(In.vLifeTime.y / max(In.vLifeTime.x, 0.0001f));
    const float surfaceCoverage = saturate(In.vInstanceColor.a * g_Tint.a);
    const float lensCoverage = EffectDistortion_ComputeLensCoverage(surfaceCoverage, 1.f, g_EffectDistortionParams.y);
    const float2 flowUV = Build_FlowUV(In.vTexcoord);
    const float2 flow = 0 != g_EffectDistortionParams.z
                        ? Sample_FlowTexture(flowUV) * 2.f - 1.f
                        : float2(0.f, 0.f);
    const float refractionIntensity =
        g_EffectDistortionParams.x * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_REFRACTION_INTENSITY, lifeProgress);
    const float2 offset = EffectDistortion_ComputeScreenOffset(flow, refractionIntensity);
    Out.color = EffectDistortion_PackField(offset, lensCoverage);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DistortionPass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, PS_MAIN)
}
