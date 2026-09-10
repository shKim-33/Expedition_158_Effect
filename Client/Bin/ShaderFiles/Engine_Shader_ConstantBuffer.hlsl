// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_ConstantBuffer.hlsl
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_ConstantBuffer.hlsl - 먼저 이 원본의 의도를 확인하세요.
#ifndef ENGINE_SHADER_CONSTANTBUFFER_HLSL
#define ENGINE_SHADER_CONSTANTBUFFER_HLSL

//cbuffer CameraInv
//{
//    float4x4 g_ViewInvMatrix;       //   0 ~  63
//    float4x4 g_ProjInvMatrix;       //  64 ~ 127
//}; // 128 bytes

cbuffer CameraBuffer
{
    row_major float4x4 g_ViewMatrix;
    row_major float4x4 g_ProjMatrix;
    row_major float4x4 g_ViewMatrixInverse;
    row_major float4x4 g_ProjMatrixInverse;

    float4 g_CamPosition;
    float g_Far;
    float3 g_CameraPadding0;
};

cbuffer ObjectBuffer
{
    row_major float4x4 g_WorldMatrix;
};

cbuffer ShadowBuffer
{
    row_major float4x4 g_LightViewMatrix;
    row_major float4x4 g_LightProjMatrix;

    row_major float4x4 g_CascadeLightViewMatrices[4];
    row_major float4x4 g_CascadeLightProjMatrices[4];

    float4 g_CascadeSplits;
    float g_CascadeBlendDistance;
    float g_LightFar;
    float g_Bias;

    float g_ShadowIntensity;
    uint g_ShadowCascadeCount;
    uint g_ActiveShadowCascadeIndex;
    float g_ShadowPadding0;

    row_major float4x4 g_CachedShadowViewProjMatrix;
    uint g_UseCachedInstancedShadow;
    float g_CachedShadowPad0;
    float g_CachedShadowPad1;
    float g_CachedShadowPad2;
};

cbuffer ScreenSpaceBuffer
{
    row_major float4x4 g_ScreenViewMatrix;          // 64
    row_major float4x4 g_ScreenProjMatrix;          // 128

    row_major float4x4 g_DeferredViewMatrixInverse; // 192
    row_major float4x4 g_DeferredProjMatrixInverse; // 256

    float4  g_DeferredCamPosition;                  // 272

    float2 g_TexelSize;                             // 272
    float2 g_ScreenSize;                            // 280
    float g_DeferredFar;                            // 288
    float3 g_DeferredPadding0;                      // 292
};

cbuffer LightBuffer
{
    float4 g_LightDir;
    float4 g_LightPos;

    float4 g_LightAmbient;
    float4 g_LightDiffuse;
    float4 g_LightSpecular;

    float g_LightRange;
    float g_PointLightAttenuationPower;
    float2 g_LightPadding0;
};

cbuffer MaterialBuffer
{
    float4 g_BaseColorTint;         //  0 ~ 15
    float4 g_EmissiveColor;         // 16 ~ 31
    float g_EmissiveIntensity;      // 32 ~ 35
    float g_EmissiveMaskMul;        // 36 ~ 39
    float g_NormalStrength;         // 40 ~ 43
    float g_OpacityMaskClipValue;   // 44 ~ 47
    float g_AoScale;                // 48 ~ 51
    float g_RoughnessScale;         // 52 ~ 55
    float g_MetalnessScale;         // 56 ~ 59
    float g_MaterialPadding0;       // 60 ~ 63
    float2 g_UVScale;               // 64 ~ 71
    float2 g_UVOffset;              // 72 ~ 79
    float g_UVRotate;               // 80 ~ 83
    uint g_iMaterialMask;           // 84 ~ 87
    uint g_OrmAoSource;             // 88 ~ 91
    uint g_OrmRoughnessSource;      // 92 ~ 95
    uint g_OrmMetalnessSource;      // 96 ~ 99
    uint g_OpacityMaskSource;       // 100 ~ 103
    uint g_OrmAoSourceSlot;         // 104 ~ 107
    uint g_OrmRoughnessSourceSlot;  // 108 ~ 111
    uint g_OrmMetalnessSourceSlot;  // 112 ~ 115
    float2 g_MaterialPadding1;      // 116 ~ 119
}; // 112 bytes

cbuffer DrawFeatureBuffer
{
    uint g_ActiveFeatureMask;
    uint g_SupportFeatureMask;
    uint g_FeatureTextureMask;
    uint g_DrawFeaturePadding0;

    float4 g_FeatureParam0;
    float4 g_FeatureParam1;
    float4 g_FeatureParam2;
    float4 g_FeatureParam3;
};

cbuffer SSAOBuffer
{
    float g_SSAORadius;
    float g_SSAOBias;
    float g_SSAOIntensity;
    float g_SSAOPower;

    float g_SSAODepthRange;
    float g_SSAOBlurRadius;
    float g_SSAODepthSharpness;
    float g_SSAONormalSharpness;

    uint g_SSAOSampleCount;
    uint g_SSAOEnabled;
    uint g_SSAOBilateralEnabled;
    uint g_SSAOPad0;
};

cbuffer DeferredLightingBuffer
{
    float g_IBLIntensity;
    float g_IBLDiffuseIntensity;
    float g_IBLSpecularIntensity;
    float g_IBLPrefilterMaxMip;

    float g_SpecAAStrength;
    float g_SpecAAMinVariance;
    float g_SpecAAMaxAddedRoughness;
    uint g_IBLEnabled;

    uint g_SpecAAEnabled;
    uint g_StylizedLightContrastEnabled;
    float g_StylizedLightContrastStrength;
    float g_StylizedLightThreshold;

    float g_StylizedLightSoftness;
    float g_StylizedLitBoost;
    float g_StylizedUnlitAmbientScale;
    float g_StylizedDeepUnlitThreshold;

    float g_StylizedDeepUnlitSoftness;
    float g_StylizedDeepUnlitAmbientScale;
    float2 g_DeferredLightingPadding0;
};

cbuffer PostProcessBuffer
{
    float g_PP_Exposure;        //  0 ~  3
    float g_PP_InvWhitePoint;   //  4 ~  7
    float g_PP_Gamma;           //  8 ~ 11
    float g_PP_BloomRadius;     // 12 ~ 15

    float g_PP_BloomThreshold;  // 16 ~ 19
    float g_PP_BloomIntensity;  // 20 ~ 23
    float g_PP_BloomSoftKnee;   // 24 ~ 27
    float g_PP_BloomClamp;      // 28 ~ 31

    float g_PP_BloomScatter;    // 32 ~ 35
    uint g_PP_ToneMapMode;      // 36 ~ 39

    uint g_PP_FeatureMask;             // 40 ~ 43
    uint g_PP_UseBackgroundFallback;   // 44 ~ 47
    float4 g_PP_Padding2;              // 48 ~ 63

    float4 g_PP_BackgroundFallbackColor; // 64 ~ 79

    float g_PP_DOFFocusDistance;       // 80 ~ 83
    float g_PP_DOFFocusRange;          // 84 ~ 87
    float g_PP_DOFNearMaxRadius;       // 88 ~ 91
    float g_PP_DOFFarMaxRadius;        // 92 ~ 95

    float g_PP_DOFBokehThreshold;      //  96 ~  99
    float g_PP_DOFBokehIntensity;      // 100 ~ 103
    float g_PP_DOFDepthSharpness;      // 104 ~ 107
    float g_PP_DOFCoCEpsilon;          // 108 ~ 111

    float g_PP_DOFNearIntensity;       // 112 ~ 115
    float g_PP_DOFFarIntensity;        // 116 ~ 119
    uint g_PP_DOFSampleCount;          // 120 ~ 123
    uint g_PP_DOFDebugMode;            // 124 ~ 127

    float4 g_PP_ColorGradeShadowTint;    // 128 ~ 143
    float4 g_PP_ColorGradeHighlightTint; // 144 ~ 159

    float g_PP_ColorGradeContrast;       // 160 ~ 163
    float g_PP_ColorGradeSaturation;     // 164 ~ 167
    float g_PP_ColorGradePivot;          // 168 ~ 171
    float g_PP_ColorGradeStrength;       // 172 ~ 175
}; // 176 bytes

cbuffer LightShaftBuffer
{
    float2 g_LS_LightScreenUV;          // 0 ~  7
    float g_LS_LightVisible;            // 8 ~ 11
    float g_LS_LightFacingCamera;       // 12 ~ 15

    float3 g_LS_ShaftColor;             // 16 ~ 27
    float g_LS_Intensity;               // 28 ~ 31

    uint g_LS_SampleCount;              // 32 ~ 35
    float g_LS_RayLength;               // 36 ~ 39
    float g_LS_Decay;                   // 40 ~ 43
    float g_LS_SunDiscRadius;           // 44 ~ 47

    float g_LS_EdgeFade;                // 48 ~ 51
    float g_LS_DepthEmptyThreshold;     // 52 ~ 55
    float2 g_LS_Padding0;               // 56 ~ 63
};  // 64 bytes

cbuffer HairDescBuffer
{
    float4 g_HairColorTop;
    float4 g_HairColorDown;
    float4 g_HairHighlightColor;

    float g_HairOpacityMaskClipValue;
    float g_HairRoughness;
    float g_HairSpecular;
    float g_HairScatter;

    float g_HairBrightness;
    float g_HairRim;
    float g_HairRandomHueVariation;
    float g_HairRandomValueVariation;

    float g_HairBumpStrength;
    float g_HairBumpDistance;
    float g_HairGradientColor;
    float g_HairGradientContrast;

    float g_HairGradientHardness;
    float g_HairGradientHeight;
    float g_HairEdgeContrast;
    float g_HairEdgeMin;
};

cbuffer ScreenFxBuffer
{
    float3 g_vignetteColor;
    float g_vignetteIntensity;
    float g_vignetteRadius;
    float g_vignetteSoftness;

    float g_ghostingIntensity;
    float g_transitionProgress;

    float3 g_transitionColor;
    float g_radialBlurIntensity;

    float2 g_radialBlurCenter;
    float g_radialBlurRadius;
    float g_chromaticAberrationIntensity;

    float g_bulgeIntensity;
    float g_pullIntensity;
    float2 g_pullDirection;

    float g_wipeRadius;
    float g_edgeSoftness;
    float g_transitionUseBlackSource;
    float pad;

    float4 g_flashColor;
    float4 g_screenFxShape;
    float4 g_screenFxParams;
};

struct FogZoneEntry
{
    float3 zoneMin;
    float baseHeight;

    float3 zoneMax;
    float topFadeStart;

    float4 fogColor;

    float fogDensity;
    float topHeight;
    float edgeFade;
    float noiseScale;

    float2 noiseSpeed;
    float noiseStrength;
    float noiseMin;
};

struct FogOBBZoneEntry
{
    row_major float4x4 worldToLocal;

    float3 halfExtents;
    float baseHeight;

    float4 fogColor;

    float fogDensity;
    float topFadeStart;
    float topHeight;
    float edgeFade;

    float noiseScale;
    float2 noiseSpeed;
    float noiseStrength;

    float noiseMin;
    float3 padding;
};

cbuffer FogZoneBuffer
{
    FogZoneEntry g_FogAABBZones[16];
    FogOBBZoneEntry g_FogOBBZones[16];
    int g_FogAABBCount;
    int g_FogOBBCount;
    float g_GroundFogMaxDistance;
    float g_GroundFogMinAccumulationDistance;

    float g_GroundFogDistanceInfluence;
    float g_GroundFogNearFadeDistance;
    float g_GroundFogNearMinFactor;
    float g_GlobalFogIntensity;
    float2 g_FogZonePad0;
};

cbuffer WaterBuffer : register(b13)
{
    float g_LargeWaveScale;
    float g_LargeWaveSpeed;
    float g_LargeWaveAmplify;
    float g_SmallWaveScale;

    float g_SmallWaveSpeed;
    float g_SmallWaveAmplify;
    float g_SeafoamScale;
    float g_SeafoamSpeed;

    float g_SeafoamHeightPower;
    float g_SeafoamHeightMultiply;
    float g_FoamDistortion;
    float g_DepthFade;

    float g_CameraDistanceFade;
    float g_ReflectionAmount;
    float g_LuminanceBias;
    float g_WaterElapsedTime;

    float4 g_WaterColor;
    float4 g_WaterFresnelColor;
}

cbuffer PlayerRenderBuffer
{
    float4 g_PlayerNearClipParams;
    // x: enabled, y: camera-space clip distance
}

cbuffer FroxelDebugBuffer
{
    uint g_FroxelDebugWidth;
    uint g_FroxelDebugHeight;
    uint g_FroxelDebugDepth;
    uint g_FroxelDebugSlice;
    uint g_FroxelDebugMode;
    float g_FroxelDebugGain;
    float2 g_FroxelDebugPad0;
};

struct VolumetricLightShaftZoneEntry
{
    row_major float4x4 worldToLocal;

    float3 halfExtents;
    float density;

    float4 lightColor;

    float edgeFade;
    float intensity;
    float2 padding;
};

// 카메라/Froxel/광원 공통 데이터
cbuffer FroxelBuffer : register(b0)
{
    uint g_FroxelWidth;
    uint g_FroxelHeight;
    uint g_FroxelDepth;
    float g_FroxelNear;

    float g_FroxelFar;
    float g_FroxelCompositeIntensity;
    float g_FroxelDepthBias;
    float g_FroxelShadowStrength;
    float g_FroxelCameraFar;
    float3 g_FroxelPadding;

    row_major float4x4 g_FroxelInvView;
    row_major float4x4 g_FroxelInvProjection;
    row_major float4x4 g_FroxelPrevView;
    row_major float4x4 g_FroxelPrevProjection;

    float4 g_FroxelLightDirection;
    float4 g_FroxelLightColor;
    float4 g_FroxelCamPosition;

    float g_FroxelTemporalBlend;
    float g_FroxelJitterZ;
    float g_FroxelTemporalDepthReject;
    uint g_FroxelHistoryValid;
}

// 카메라/Froxel/광원 공통 데이터
cbuffer VolumetricLightShaftZoneBuffer : register(b1)
{
    VolumetricLightShaftZoneEntry g_LightShaftZones[16];

    uint g_LightShaftZoneCount;
    float3 g_LightShaftZonePadding;
}

cbuffer FroxelShadowBuffer : register(b2)
{
    row_major float4x4 g_FroxelLightViewMatrix;
    row_major float4x4 g_FroxelLightProjMatrix;

    row_major float4x4 g_FroxelCascadeLightViewMatrices[4];
    row_major float4x4 g_FroxelCascadeLightProjMatrices[4];

    float4 g_FroxelCascadeSplits;
    float g_FroxelCascadeBlendDistance;
    float g_FroxelLightFar;
    float g_FroxelShadowBias;

    float g_FroxelShadowIntensity;
    uint g_FroxelShadowCascadeCount;
    uint g_FroxelActiveShadowCascadeIndex;
    float g_FroxelShadowPadding0;
};

#endif
