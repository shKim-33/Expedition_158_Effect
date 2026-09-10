#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectMeshPreviewDistortion : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f }; // x:intensity, y:presence, z:useFlow, w:material time
    float4 g_DistortionScreenSize = { 512.f, 512.f, 0.f, 0.f };
    float2 g_FlowUVPolicyParams = { 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float g_FlowUVTilingMode = 0.f;
    float g_FlowUVRotation = 0.f;
    float g_EffectMeshPreviewDistortionPadding = 0.f;
};

Texture2D g_FlowTexture : register(t3);
Texture2D g_DistortionSourceTexture : register(t4);

BlendState BS_EffectMeshPreviewDistortionAlphaBlend
{
    BlendEnable[0] = true;

    SrcBlend = Src_Alpha;
    DestBlend = Inv_Src_Alpha;
    BlendOp = Add;

    SrcBlendAlpha = One;
    DestBlendAlpha = Inv_Src_Alpha;
    BlendOpAlpha = Add;
};

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
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
    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexcoord = In.vTexcoord;
    return Out;
}

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

PS_OUT PS_MAIN(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 flowUV = Build_FlowUV(In.vTexcoord);
    const bool useFlow = 0 != g_EffectDistortionParams.z;
    const float2 flow = useFlow ? Sample_FlowTexture(flowUV) * 2.f - 1.f : float2(0.f, 0.f);
    const float lensCoverage = EffectDistortion_ComputeLensCoverage(g_Tint.a, 1.f, g_EffectDistortionParams.y);
    const float2 offset = EffectDistortion_ComputeScreenOffset(flow, g_EffectDistortionParams.x);
    const float2 screenUV = In.vPosition.xy / max(g_DistortionScreenSize.xy, float2(1.f, 1.f));
    const float4 distortedSource = g_DistortionSourceTexture.Sample(LinearClampSampler, screenUV + offset);
    const float edgeAlpha = EffectDistortion_ComputeLensEdgeAlpha(lensCoverage);

    Out.color = float4(distortedSource.rgb * g_Tint.rgb, edgeAlpha);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(DistortionPreviewPass, RS_CullNone, DSS_DepthRead, BS_EffectMeshPreviewDistortionAlphaBlend, VS_MAIN, PS_MAIN)
}
