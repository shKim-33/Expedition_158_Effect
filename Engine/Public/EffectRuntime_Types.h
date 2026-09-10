#pragma once
#include <array>
#include <variant>
#include "EffectMaterial_Types.h"
#include "Engine_Typedef.h"
#include "GameObject.h"
#include "Particle_Types.h"

NS_BEGIN(Engine)

//## Emitter::Kind

// Effect 내부 emitter의 concrete runtime 종류를 구분
enum class EffectEmitterKind : uint8
{
    Sprite,                   // compute-first sprite emitter runtime
    Trail,                    // compute-first trail emitter runtime
    Ribbon,                   // source/root position history 기반 camera-facing ribbon runtime
    SourceHistorySpriteTrail, // source/root position history 기반 tangent-aligned sprite stamp runtime
    Beam,                     // authored local path 기반 generated beam runtime
    Mesh,                     // model mesh를 instance buffer로 그리는 mesh emitter runtime
};

// Emitter 단위로 자동 render group 판정을 덮을 수 있는 제한된 override
enum class EffectEmitterRenderLayerOverride : uint8
{
    Auto,
    UIEffect,
};

enum class EffectHistoryBudgetPreset : uint8
{
    Full,
    High,
    Medium,
    Performance,
    Low,
};

enum class EffectHistoryBudgetPriority : uint8
{
    Core,
    Secondary,
    Decorative,
    Distortion,
};

enum class EffectHistoryBudgetDensityBias : uint8
{
    Dense,
    Normal,
    Sparse,
};

enum class EffectHistoryBudgetUpdateRate : uint8
{
    EveryFrame,
    Every2Frames,
};

enum class EffectHistoryBudgetFamily : uint8
{
    Trail,
    Ribbon,
    SourceHistorySpriteTrail,
    Distortion,
};

struct EffectHistoryBudgetRuntimeDesc
{
    EffectHistoryBudgetPreset preset{ EffectHistoryBudgetPreset::Full };
    bool preserveLength{ true };
    bool sharedSourceHistory{ true };
    float trailDensityScale{ 1.f };
    float spriteStampDensityScale{ 1.f };
    float ribbonDensityScale{ 1.f };
    float distortionDensityScale{ 1.f };
    EffectHistoryBudgetUpdateRate updateRate{ EffectHistoryBudgetUpdateRate::EveryFrame };
};

struct EffectEmitterHistoryBudgetRuntimeDesc
{
    EffectHistoryBudgetPriority priority{ EffectHistoryBudgetPriority::Core };
    EffectHistoryBudgetDensityBias densityBias{ EffectHistoryBudgetDensityBias::Normal };
    float lengthScale{ 1.f };
};

struct EffectHistoryBudgetEffectiveInput
{
    EffectHistoryBudgetFamily family{ EffectHistoryBudgetFamily::Trail };
    uint32 sourceSampleCount{};
    uint32 renderSegmentCount{};
    uint32 spriteStampCount{};
};

struct EffectHistoryBudgetEffectiveDesc
{
    EffectHistoryBudgetFamily family{ EffectHistoryBudgetFamily::Trail };
    uint32 sourceSampleCount{};
    uint32 renderSegmentCount{};
    uint32 spriteStampCount{};
};

enum class EffectHistorySourceGroupKind : uint8
{
    None,
    TrailPairHistory,
    SourcePointHistory,
};

struct EffectHistorySourceGroupKey
{
    EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
    string stableId{};
};

struct EffectHistorySourceGroupStats
{
    EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
    uint32 sourceRequestCount{};
    uint32 cacheHitCount{};
    uint32 savedRequestCount{};
};

// Effect root가 소유하는 source history group 단위 budget
struct EffectHistorySourceGroupBudgetDesc
{
    EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
    string stableId{};
    bool confirmed{};

    uint32 sourceSampleCount{};
    float sampleLifetime{};
    float sampleSpacing{};
    float sampleInterval{};
    uint32 curveSubdivision{};
    bool smoothTangent{ true };
};

inline const EffectHistorySourceGroupBudgetDesc* Find_EffectHistorySourceGroupBudget(
    const vector<EffectHistorySourceGroupBudgetDesc>& budgets,
    EffectHistorySourceGroupKind kind,
    const string& stableId)
{
    for (const EffectHistorySourceGroupBudgetDesc& budget : budgets)
    {
        if (budget.kind == kind && budget.stableId == stableId)
            return &budget;
    }

    return nullptr;
}

inline float Resolve_EffectHistoryBudgetPresetScale(EffectHistoryBudgetPreset preset)
{
    switch (preset)
    {
    case EffectHistoryBudgetPreset::High:
        return 0.85f;
    case EffectHistoryBudgetPreset::Medium:
        return 0.7f;
    case EffectHistoryBudgetPreset::Performance:
        return 0.5f;
    case EffectHistoryBudgetPreset::Low:
        return 0.35f;
    case EffectHistoryBudgetPreset::Full:
    default:
        return 1.f;
    }
}

inline float Resolve_EffectHistoryBudgetFamilyScale(
    const EffectHistoryBudgetRuntimeDesc& budget,
    EffectHistoryBudgetFamily family)
{
    switch (family)
    {
    case EffectHistoryBudgetFamily::Ribbon:
        return budget.ribbonDensityScale;
    case EffectHistoryBudgetFamily::Distortion:
        return budget.distortionDensityScale;
    case EffectHistoryBudgetFamily::SourceHistorySpriteTrail:
    case EffectHistoryBudgetFamily::Trail:
    default:
        return budget.trailDensityScale;
    }
}

inline float Resolve_EffectHistoryBudgetBiasScale(EffectHistoryBudgetDensityBias bias)
{
    switch (bias)
    {
    case EffectHistoryBudgetDensityBias::Dense:
        return 1.25f;
    case EffectHistoryBudgetDensityBias::Sparse:
        return 0.75f;
    case EffectHistoryBudgetDensityBias::Normal:
    default:
        return 1.f;
    }
}

inline uint32 Scale_EffectHistoryBudgetCount(uint32 value, float scale)
{
    if (value == 0u)
        return 0u;

    const float scaled = static_cast<float>(value) * scale;
    return static_cast<uint32>(scaled + 0.999f) < 1u ? 1u : static_cast<uint32>(scaled + 0.999f);
}

inline EffectHistoryBudgetEffectiveDesc Compute_EffectHistoryBudgetEffective(
    const EffectHistoryBudgetRuntimeDesc& budget,
    const EffectEmitterHistoryBudgetRuntimeDesc& usage,
    const EffectHistoryBudgetEffectiveInput& input)
{
    EffectHistoryBudgetEffectiveDesc effective{};
    effective.family = input.family;

    if (budget.preset == EffectHistoryBudgetPreset::Full)
    {
        effective.sourceSampleCount = input.sourceSampleCount;
        effective.renderSegmentCount = input.renderSegmentCount;
        effective.spriteStampCount = input.spriteStampCount;
        return effective;
    }

    const float scale =
        Resolve_EffectHistoryBudgetPresetScale(budget.preset) *
        Resolve_EffectHistoryBudgetFamilyScale(budget, input.family) *
        Resolve_EffectHistoryBudgetBiasScale(usage.densityBias);

    effective.sourceSampleCount = Scale_EffectHistoryBudgetCount(input.sourceSampleCount, scale);
    effective.renderSegmentCount = Scale_EffectHistoryBudgetCount(input.renderSegmentCount, scale);
    effective.spriteStampCount = input.spriteStampCount;
    return effective;
}

enum class EffectSortPolicy : uint8
{
    None,
    EmitterDepth,
};

//## Trail::SampleAndProvider

// Trail runtime이 한 프레임에서 소비할 base/tip world sample
struct EffectTrailSample
{
    Vec3 baseWorldPosition{}; // trail 폭의 시작 기준점
    Vec3 tipWorldPosition{};  // trail 폭의 끝 기준점
    Quat baseWorldRotation{ Quat::Identity };
    Quat tipWorldRotation{ Quat::Identity };
};

// Source history 계열 runtime이 한 프레임에서 소비할 단일 source point sample
struct EffectSourcePointSample
{
    Vec3 worldPosition{}; // source history head로 기록할 world position
    Quat worldRotation{ Quat::Identity };
};

// Trail emitter가 attachment owner에게서 sample만 읽기 위한 좁은 provider 계약
class ENGINE_DLL IEffectTrailSampleProvider
{
public:
    virtual ~IEffectTrailSampleProvider() = default;
    virtual bool Try_GetTrailSample(EffectTrailSample& outSample) const = 0;
};

// Source history 계열 emitter가 attachment owner에게서 단일 point sample만 읽기 위한 좁은 provider 계약
class ENGINE_DLL IEffectSourcePointSampleProvider
{
public:
    virtual ~IEffectSourcePointSampleProvider() = default;
    virtual bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const = 0;
};

//## Module::SpriteAlignment

// Effect runtime이 어떤 기준으로 sprite/mesh facing 축을 고를지 결결정
enum class EffectScreenAlignment : uint32
{
    FacingCameraPosition = 0, // particle 위치에서 camera position을 바라보는 기본 billboard
    Rectangle = 1,            // 카메라 화면 평면 right/up을 따라가는 직사각형 billboard
    Square = 2,               // 카메라 화면 평면 기준에서 더 큰 축으로 정사각형을 생성
    AwayFromCenter = 3,       // emitter 중심에서 particle 바깥으로 향하는 radial 방향을 기준 방향으로 사용
    Velocity = 4,             // compute가 기록한 현재 속도 방향을 기준 방향으로 사용
    WorldPlaneXY = 5,         // 월드 XY 평면에 고정되는 2D plane 정렬
    WorldPlaneXZ = 6,         // 월드 XZ 평면에 고정되는 바닥형 2D plane 정렬
    WorldUpFacingCamera = 7,  // 카드 Y축을 World Y로 세우고 카메라 가시성을 유지하는 billboard
};

// AwayFromCenter/Velocity가 기준 방향을 어떻게 쓸지 결결정
enum class EffectSpriteDirectionalAlignmentMode : uint32
{
    LookDirection = 0, // 기준 방향을 sprite normal/look 방향으로 사용
    TextureAxis = 1,   // camera-facing 평면을 유지하고 texture 축만 기준 방향에 정렬
};

// TextureAxis mode에서 기준 방향에 맞출 texture 축을 선택
enum class EffectSpriteTextureAxis : uint32
{
    X = 0, // texture의 가로 축을 기준 방향에 정렬
    Y = 1, // texture의 세로 축을 기준 방향에 정렬
};

//## Module::MaterialModulation

// Material scalar modulation이 조절할 기본 material scalar field
enum class EffectMaterialScalarModulationTarget : uint32
{
    Intensity = 0,
    OpacityPower = 1,
    NoiseStrength = 2,
    AlphaErosion = 3,
    AlphaCutoff = 4,
    CoreIntensity = 5,
    OuterIntensity = 6,
    CoreColor = 7,
    RefractionIntensity = 8,
    MainUVScrollSpeedScale = 9,
    NoiseUVScrollSpeedScale = 10,
    MaskUVScrollSpeedScale = 11,
    FlowUVScrollSpeedScale = 12,
    AlphaMultiplier = 13,
};

// Material parameter modulation이 조절할 Vec2 material field
enum class EffectMaterialVec2ModulationTarget : uint32
{
    MainUVOffset = 0,
    NoiseUVOffset = 1,
    MaskUVOffset = 2,
    FlowUVOffset = 3,
};

// Material Vec2 modulation의 base value 적용 방식
enum class EffectMaterialVec2ModulationOperation : uint32
{
    Add = 0,
};

// Material scalar modulation의 base value 적용 방식
enum class EffectMaterialScalarModulationOperation : uint32
{
    Multiply = 0,
};

// Material scalar modulation이 평가할 시간 축
enum class EffectMaterialScalarModulationTimeSource : uint32
{
    ParticleLife = 0,
    EmitterTime = 1,
};

// Material scalar modulation의 float curve payload
struct EffectMaterialScalarModulationCurveDesc
{
    bool enabled{ false };
    uint32 keyCount{ 2 };
    Vec4 keyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 keyTimesBlock1{};
    Vec4 keyValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 keyValuesBlock1{};
    Vec4 keyArriveTangents{};
    Vec4 keyArriveTangentsBlock1{};
    Vec4 keyLeaveTangents{};
    Vec4 keyLeaveTangentsBlock1{};
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 keyModesBlock1{};
};

struct EffectMaterialScalarModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialScalarModulationTarget targetField{ EffectMaterialScalarModulationTarget::Intensity };
    EffectMaterialScalarModulationOperation operation{ EffectMaterialScalarModulationOperation::Multiply };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::ParticleLife };
    EffectMaterialScalarModulationCurveDesc curve{};
};

// Material scalar modulation module에서 shader/renderer가 공유할 compact payload
struct EffectMaterialScalarModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialScalarModulatorRuntimeDesc, kMaxModulators> modulators{};
};

// Core color RGB를 emitter time 기준으로 교체하기 위한 Vec3 curve payload
struct EffectMaterialCoreColorRgbModulationCurveDesc
{
    bool enabled{ false };
    bool uniformEnabled{ false };        // Uniform 분포일 때 emitter/material instance 단위 RGB sample payload를 사용
    Vec3 uniformMin{ 1.f, 0.85f, 0.45f };
    Vec3 uniformMax{ 1.f, 0.85f, 0.45f };
    PointParticleRandomSeedRuntimeDesc uniformSeed{};
    uint32 keyCount{ 2 };
    Vec4 keyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 keyTimesBlock1{};
    Vec4 keyValuesR{ 1.f, 1.f, 0.f, 0.f };
    Vec4 keyValuesRBlock1{};
    Vec4 keyValuesG{ 0.85f, 0.85f, 0.f, 0.f };
    Vec4 keyValuesGBlock1{};
    Vec4 keyValuesB{ 0.45f, 0.45f, 0.f, 0.f };
    Vec4 keyValuesBBlock1{};
    Vec4 keyArriveTangentsR{};
    Vec4 keyArriveTangentsRBlock1{};
    Vec4 keyArriveTangentsG{};
    Vec4 keyArriveTangentsGBlock1{};
    Vec4 keyArriveTangentsB{};
    Vec4 keyArriveTangentsBBlock1{};
    Vec4 keyLeaveTangentsR{};
    Vec4 keyLeaveTangentsRBlock1{};
    Vec4 keyLeaveTangentsG{};
    Vec4 keyLeaveTangentsGBlock1{};
    Vec4 keyLeaveTangentsB{};
    Vec4 keyLeaveTangentsBBlock1{};
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 keyModesBlock1{};
};

struct EffectMaterialCoreColorRgbModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::EmitterTime };
    EffectMaterialCoreColorRgbModulationCurveDesc curve{};
};

// Core color RGB distribution module에서 renderer가 CPU로 평가할 compact payload
struct EffectMaterialCoreColorRgbModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialCoreColorRgbModulatorRuntimeDesc, kMaxModulators> modulators{};
};

// Material parameter modulation의 Vec2 curve payload
struct EffectMaterialVec2ModulationCurveDesc
{
    bool enabled{ false };
    uint32 keyCount{ 2 };
    Vec4 keyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 keyTimesBlock1{};
    Vec4 keyValuesX{ 0.f, 0.f, 0.f, 0.f };
    Vec4 keyValuesXBlock1{};
    Vec4 keyValuesY{ 0.f, 0.f, 0.f, 0.f };
    Vec4 keyValuesYBlock1{};
    Vec4 keyArriveTangentsX{};
    Vec4 keyArriveTangentsXBlock1{};
    Vec4 keyArriveTangentsY{};
    Vec4 keyArriveTangentsYBlock1{};
    Vec4 keyLeaveTangentsX{};
    Vec4 keyLeaveTangentsXBlock1{};
    Vec4 keyLeaveTangentsY{};
    Vec4 keyLeaveTangentsYBlock1{};
    Vec4 keyModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 keyModesBlock1{};
};

struct EffectMaterialVec2ModulatorRuntimeDesc
{
    bool enabled{ true };
    EffectMaterialVec2ModulationTarget targetField{ EffectMaterialVec2ModulationTarget::MainUVOffset };
    EffectMaterialVec2ModulationOperation operation{ EffectMaterialVec2ModulationOperation::Add };
    EffectMaterialScalarModulationTimeSource timeSource{ EffectMaterialScalarModulationTimeSource::EmitterTime };
    EffectMaterialVec2ModulationCurveDesc curve{};
};

// Material parameter modulation module에서 renderer가 CPU로 평가할 Vec2 compact payload
struct EffectMaterialVec2ModulationRuntimeDesc
{
    static constexpr uint32 kMaxModulators{ 8 };

    uint32 count{ 0 };
    std::array<EffectMaterialVec2ModulatorRuntimeDesc, kMaxModulators> modulators{};
};

//## Module::Required

// Required 모듈에서 material binding에 필요한 값만 접은 runtime payload
struct EffectRequiredMaterialRuntimeDesc
{
    EffectMaterialFamily materialFamily{
        EffectMaterialFamily::Default }; // runtime material shader/pass family를 고르는 분류
    string mainTextureGuid{};            // main particle texture asset identity
    string mainTexturePath{};            // main particle texture resource-relative fallback/debug hint
    string noiseTextureGuid{};           // (선택) noise texture asset identity
    string noiseTexturePath{};           // (선택) noise texture resource-relative fallback/debug hint
    string maskTextureGuid{};            // (선택) mask texture asset identity
    string maskTexturePath{};            // (선택) mask texture resource-relative fallback/debug hint
    string flowTextureGuid{};            // SpriteDistortion flow texture asset identity
    string flowTexturePath{};            // SpriteDistortion flow texture resource-relative fallback/debug hint

    EffectMaterialBlendMode blendMode{
        EffectMaterialBlendMode::AlphaBlend }; // effect material blend/compose mode
    string opacitySource{ "Alpha" };           // opacity source selector
    Vec4 tint{ 1.f, 1.f, 1.f, 1.f };           // texture sample에 곱할 tint
    float intensity{ 1.f };                    // sprite 색상 밝기/발광감 계수
    float opacityPower{ 1.f };                 // opacity sharpening 계수
    float alphaMultiplier{ 1.f };              // 최종 effect alpha에 곱할 투명도 배율
    float noiseStrength{ 0.f };                // noise modulation 강도
    float alphaCutoff{ 0.f };                  // Trail 등 지원 renderer가 사용할 alpha discard threshold
    float alphaErosion{ 0.f };                 // mask/noise 기반 alpha erosion 후보 값
    string noiseSource{ "Red" };               // noise texture scalar source selector
    string maskSource{ "Alpha" };              // mask texture coverage source selector
    bool noiseInvert{ false };                 // noise scalar를 1-x로 반전
    bool maskInvert{ false };                  // mask coverage를 1-x로 반전

    Vec2 mainUVScale{ 1.f, 1.f };          // main texture UV 반복/확대 배율 후보 값
    Vec2 mainUVOffset{ 0.f, 0.f };         // main texture 샘플 시작 위치를 정적으로 이동
    Vec2 mainUVScrollSpeed{ 0.f, 0.f };    // main texture 샘플 좌표를 시간에 따라 밀어 흐름감을 만드는 속도 후보 값
    EffectTextureUVTilingMode mainUVTilingMode{
        EffectTextureUVTilingMode::Wrap }; // main texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy mainUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap }; // main texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation mainUVRotation{
        EffectMaterialUVRotation::None };  // main texture UV를 90도 단위로 회전하는 방식

    Vec2 noiseUVScale{ 1.f, 1.f };         // noise texture UV 반복/확대 배율 후보 값
    Vec2 noiseUVOffset{ 0.f, 0.f };        // noise texture 샘플 시작 위치를 정적으로 이동
    Vec2 noiseUVScrollSpeed{ 0.f, 0.f };   // noise를 frame 교체가 아니라 UV 좌표 이동으로 흘려 불규칙한 modulation 흐름을 만드는 속도 후보 값
    EffectTextureUVTilingMode noiseUVTilingMode{
        EffectTextureUVTilingMode::Wrap }; // noise texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy noiseUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap }; // noise texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation noiseUVRotation{
        EffectMaterialUVRotation::None };  // noise texture UV를 90도 단위로 회전하는 방식

    Vec2 maskUVScale{ 1.f, 1.f };          // mask texture UV 반복/확대 배율 후보 값
    Vec2 maskUVOffset{ 0.f, 0.f };         // mask texture 샘플 시작 위치를 정적으로 이동
    Vec2 maskUVScrollSpeed{ 0.f, 0.f };    // mask를 frame 교체가 아니라 UV 좌표 이동으로 흘려 alpha/erosion 변화를 만드는 속도 후보 값
    EffectTextureUVTilingMode maskUVTilingMode{
        EffectTextureUVTilingMode::Wrap }; // mask texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy maskUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap }; // mask texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation maskUVRotation{
        EffectMaterialUVRotation::None };  // mask texture UV를 90도 단위로 회전하는 방식

    Vec2 flowUVScale{ 1.f, 1.f };          // SpriteDistortion flow texture UV 반복/확대 배율 후보 값
    Vec2 flowUVOffset{ 0.f, 0.f };         // SpriteDistortion flow texture 샘플 시작 위치를 정적으로 이동
    Vec2 flowUVScrollSpeed{ 0.f, 0.f };    // SpriteDistortion flow texture UV scroll speed 후보 값
    EffectTextureUVTilingMode flowUVTilingMode{
        EffectTextureUVTilingMode::Wrap }; // flow texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy flowUVPolicy{
        EffectTextureUVTilingMode::Wrap,
        EffectTextureUVTilingMode::Wrap }; // flow texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation flowUVRotation{
        EffectMaterialUVRotation::None };  // flow texture UV를 90도 단위로 회전하는 방식

    EffectMaterialAdditiveContributionData additive{};                        // Additive blend에서 더할 contribution 해석 규칙
    EffectMaterialCoreEmissiveData coreEmissive{};                            // alpha-derived inner core/outer halo shaping 값
    float refractionIntensity{ 0.f };                                         // SpriteDistortion screen UV offset 강도
    float refractionPresence{ 1.f };                                          // SpriteDistortion refraction 영향도 scalar
    EffectDistortionShapeMode distortionShapeMode{
        EffectDistortionShapeMode::None };                                    // SpriteDistortion 영향 범위 weight mode
    EffectAirSheathMapInterpretation airSheathMapInterpretation{
        EffectAirSheathMapInterpretation::VectorField };
    EffectAirSheathMapScalarSource airSheathMapXSource{
        EffectAirSheathMapScalarSource::Red };
    EffectAirSheathMapScalarSource airSheathMapYSource{
        EffectAirSheathMapScalarSource::Green };
    EffectAirSheathMapVectorSpace airSheathMapVectorSpace{
        EffectAirSheathMapVectorSpace::Screen };
    EffectAirSheathMapComposition airSheathMapComposition{
        EffectAirSheathMapComposition::Replace };
    float airSheathMapInfluence{ 0.f };
    float distortionShapeRadius{ 0.45f };                                     // sprite UV 중심에서 shape 기준 반경
    float distortionShapeThickness{ 0.10f };                                  // Ring은 UV 폭, ShockwaveRing은 screen pixel half-width
    float distortionShapeSoftness{ 0.15f };                                   // Ring은 UV falloff, ShockwaveRing은 screen pixel falloff
    float glassAlpha{ 0.18f };                                                // MeshGlass 기본 표면 투명도
    float glassAlphaPower{ 1.f };                                             // MeshGlass texture coverage sharpen/soften 보정
    float glassNormalStrength{ 1.f };                                         // MeshGlass model normal 반응 강도
    Color glassRimColor{ 1.f, 1.f, 1.f, 1.f };                                // MeshGlass view-angle rim 색상
    float glassRimIntensity{ 1.5f };                                          // MeshGlass rim 밝기 배율
    float glassRimPower{ 3.f };                                               // MeshGlass rim falloff 곡선의 날카로움
    Vec3 glassLightDirection{ -0.35f, 0.65f, -0.65f };                        // MeshGlass world-space fake light 방향
    Color glassLightColor{ 1.f, 1.f, 1.f, 1.f };                              // MeshGlass fake light 색상
    float glassLightIntensity{ 1.2f };                                        // MeshGlass fake light 밝기 배율
    float glassSpecularPower{ 48.f };                                         // MeshGlass highlight 날카로움
    float glassSpecularSoftness{ 1.f };                                       // MeshGlass highlight 폭 보정
    float glassMainInfluence{ 0.25f };                                        // MeshGlass main texture tint/pattern 반영 강도
    float glassNoiseBreakup{ 0.35f };                                         // MeshGlass noise texture breakup 강도
    float glassMaskStrength{ 1.f };                                           // MeshGlass mask coverage 강도
    bool twoSided{ false };                                                   // 지원 renderer가 사용할 양면 렌더 힌트
    uint32 subUVRows{ 1 };                                                    // flipbook row 수
    uint32 subUVCols{ 1 };                                                    // flipbook column 수
    EffectMaterialCoreColorRgbModulationRuntimeDesc coreColorRgbModulation{}; // coreColor.rgb emitter-time distribution payload
    EffectMaterialVec2ModulationRuntimeDesc vec2Modulation{};                 // material Vec2 emitter-time distribution payload
    EffectMaterialScalarModulationRuntimeDesc scalarModulation{};             // material scalar modulation compact payload
};

// Required 모듈에서 sprite render alignment에 필요한 값만 접은 runtime payload
struct EffectSpriteRenderRuntimeDesc
{
    EffectScreenAlignment screenAlignment{ EffectScreenAlignment::FacingCameraPosition };                               // sprite 축 선택 기준
    EffectSpriteDirectionalAlignmentMode directionalAlignmentMode{ EffectSpriteDirectionalAlignmentMode::TextureAxis }; // 방향 기준 사용 방식
    EffectSpriteTextureAxis spriteTextureAxis{ EffectSpriteTextureAxis::Y }; // TextureAxis mode에서 맞출 texture 축
    float spriteRollOffsetDegrees{ 0.f };                                    // authored texture 방향 차이를 보정하는 정적 roll offset
};

// Required 모듈에서 emitter playback에 필요한 값만 접은 runtime payload
struct EffectEmitterPlaybackRuntimeDesc
{
    float duration{ 1.f };            // emitter 한 loop가 재생되는 시간
    uint32 loopCount{ 0 };            // 0은 무한 반복, 1 이상은 지정 횟수 재생
    float delay{ 0.f };               // loop 시작 전 spawn/draw를 막는 지연 시간
    bool delayFirstLoopOnly{ false }; // true면 첫 loop에만 delay를 적용
    bool killOnDeactivate{ false };   // 외부 deactivate 계약이 생기면 active particle을 즉시 죽일지 여부
    bool killOnCompleted{ false };    // 마지막 loop 완료 시 active particle을 즉시 죽일지 여부
};

// Required 모듈에서 draw 수 제한에 필요한 값만 접은 runtime payload
struct EffectDrawLimitRuntimeDesc
{
    bool useMaxDrawCount{ true }; // maxDrawCount를 draw 상한으로 적용할지 여부
    uint32 maxDrawCount{ 500 };   // 동시에 그릴 particle 수 상한. buffer capacity는 바꾸지 않음
};

// Required 모듈에서 Blend 정렬 보정에 필요한 값만 접은 runtime payload
struct EffectRequiredSortRuntimeDesc
{
    EffectSortPolicy sortPolicy{ EffectSortPolicy::None }; // 같은 sort layer 안에서 어떤 Blend 정렬을 적용할지 결결정
    int32 sortLayer{ 0 };                                  // 낮은 layer 먼저, 높은 layer 나중에 그리는 순서
    float artistSortBias{ 0.f };                           // 양수는 더 앞, 음수는 더 뒤로 보정하려는 제작자 의도 값
};

// Required 모듈의 runtime 소비 payload를 의미별로 묶은 desc
struct EffectRequiredRuntimeDesc
{
    EffectRequiredMaterialRuntimeDesc material{};
    EffectSpriteRenderRuntimeDesc spriteRender{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectDrawLimitRuntimeDesc drawLimit{};
    EffectRequiredSortRuntimeDesc sort{};
};

//## Module::CommonRuntime

// Spawn 모듈의 runtime 소비 payload와 spawn fallback shape를 묶은 desc
struct EffectSpawnRuntimeDesc
{
    uint32 instanceCount{ 96 };             // 생성할 particle 수
    Vec3 spawnRange{ 1.f, 1.f, 1.f };       // Phase2 spawn shape 전까지 사용할 기본 box spawn 범위
    PointParticleSpawnDesc particleSpawn{}; // Spawn 모듈이 만든 연속/버스트 생성 규칙
};

// Lifetime 모듈에서 compute payload에 필요한 값만 접은 runtime payload
struct EffectLifetimeRuntimeDesc
{
    Vec2 lifeTime{ 0.5f, 1.f };                         // compute payload가 사용할 생존 시간 범위
    PointParticleFloatCurveRuntimeDesc lifeTimeCurve{}; // Lifetime ConstantCurve를 sampling event phase 기준으로 평가하는 payload
};

// InitialSize 모듈에서 compute payload에 필요한 값만 접은 runtime payload
struct EffectInitialSizeRuntimeDesc
{
    bool enabled{ false };        // InitialSize 모듈이 runtime에 연결되었는지 여부
    Vec2 sizeMin{ 0.12f, 0.12f }; // InitialSize의 가로/세로 크기 하한
    Vec2 sizeMax{ 0.28f, 0.28f }; // InitialSize의 가로/세로 크기 상한
    PointParticleRandomSeedRuntimeDesc sizeSeed{};
};

// InitialColor 모듈에서 compute payload에 필요한 값만 접은 runtime payload
struct EffectInitialColorRuntimeDesc
{
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f }; // Initial Color 하한
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f }; // Initial Color 상한
    PointParticleRandomSeedRuntimeDesc colorSeed{};
    PointParticleRandomSeedRuntimeDesc alphaSeed{};
};

// ColorOverLife 모듈에서 compute payload에 필요한 값만 접은 runtime payload
struct EffectColorOverLifeRuntimeDesc
{
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };      // Color Over Life 하한
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };      // Color Over Life 상한
    PointParticleColorOverLifeCurveDesc curve{}; // ConstantCurve ColorOverLife runtime payload
    PointParticleRandomSeedRuntimeDesc colorSeed{};
    PointParticleRandomSeedRuntimeDesc alphaSeed{};
};

// Trail 전용 SpawnPerUnit 모듈에서 이동 거리 기반 보강 sample 생성에 필요한 값만 접은 payload
struct EffectTrailSpawnPerUnitRuntimeDesc
{
    bool enabled{ false };          // true면 이동 거리 기준으로 history sample을 보강 삽입
    float spawnPerUnit{ 0.f };      // unitScalar 기준 단위 거리당 생성할 sample 수
    float unitScalar{ 1.f };        // spawnPerUnit을 실제 world distance로 환산하는 스칼라
    float movementTolerance{ 0.f }; // 이 거리 미만 이동은 새 sample을 만들지 않음
    float maxFrameDistance{ 0.f };  // 이 거리보다 큰 frame 이동은 teleport/skip으로 보고 history를 재시드. 0 이하면 제한하지 않음
};

//## Emitter::RuntimeDesc

// compute sprite emitter가 실제 runtime consumer에게 넘길 module-facing payload
struct ComputeSpriteEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    PointParticleDrawMode drawMode{
        PointParticleDrawMode::DrawIndexedInstancedIndirect };        // compute sprite 정식 draw 제출 방식
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };                     // Auto면 material/type 기반 render group을 사용
    EffectRequiredRuntimeDesc required{};                             // Required 모듈에서 runtime이 소비하는 값
    EffectSpawnRuntimeDesc spawn{};                                   // Spawn 모듈에서 runtime이 소비하는 값
    EffectLifetimeRuntimeDesc lifetime{};                             // Lifetime 모듈에서 runtime이 소비하는 값
    EffectInitialSizeRuntimeDesc initialSize{};                       // InitialSize 모듈에서 runtime이 소비하는 값
    EffectInitialColorRuntimeDesc initialColor{};                     // InitialColor 모듈에서 runtime이 소비하는 값
    EffectColorOverLifeRuntimeDesc colorOverLife{};                   // ColorOverLife 모듈에서 runtime이 소비하는 값
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};         // 수명 진행률 기준 SubUV frame 규칙
    PointParticleSizeByLifeDesc sizeByLife{};                         // 수명 진행률 기준 크기 배율 규칙
    PointParticleMotionDesc motion{};                                 // motion authoring module을 compute payload로 압축한 규칙
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};                   // OrbitOverLife 모듈이 만든 emitter-local pivot 회전 payload
    PointParticleRotationDesc rotation{};                             // rotation authoring module을 compute payload로 압축한 규칙
    PointParticleSpriteTiltDesc spriteTilt{};                         // SpriteTilt authoring module을 compute payload로 압축한 규칙
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    PointParticleInitialLocationDesc initialLocation{};               // InitialLocation 모듈이 만든 시작 위치 offset 분포
    PointParticleSphereLocationDesc sphereLocation{};                 // SphereLocation 모듈이 만든 시작 위치 구형 offset 분포
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};       // PlaneRadialLocation 모듈이 만든 시작 위치 평면/방사 offset 분포
    PointParticleCylinderLocationDesc cylinderLocation{};             // CylinderLocation 모듈이 만든 시작 위치 원통 offset 분포
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{}; // PlaneRadialOrientation 모듈이 만든 radial frame 기반 초기 회전 payload
    PointParticleCylinderOrientationDesc cylinderOrientation{};       // CylinderOrientation 모듈이 만든 cylinder frame 기반 초기 방향 payload
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};
    Vec3 center{ 0.f, 0.f, 0.f }; // concrete emitter 내부 fallback/초기 center. root transform이 있으면 후속 sync가 덮어씀
};

// compute trail emitter가 clone 시점에 받는 trail 전용 runtime payload
struct ComputeTrailEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };      // Auto면 material family 기반 render group을 사용
    EffectRequiredMaterialRuntimeDesc material{};      // trail 기본 material/tint/blend 값
    EffectEmitterPlaybackRuntimeDesc playback{};       // duration/loop/delay 같은 공통 재생 규칙
    EffectLifetimeRuntimeDesc lifetime{};              // Lifetime 모듈에서 trail이 소비하는 age fade 기준 값
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{}; // Lifetime replay 단위 segment lifetime random sample seed salt
    bool useLifetimeSegmentLifetime{ false };          // true면 Lifetime 모듈을 segmentLifetime 기준으로 소비
    EffectInitialColorRuntimeDesc initialColor{};      // InitialColor 모듈에서 trail segment 시작 색상으로 소비하는 값
    EffectColorOverLifeRuntimeDesc colorOverLife{};    // ColorOverLife 모듈에서 trail segment 끝 색상으로 소비하는 값
    PointParticleSizeByLifeDesc sizeByLife{};          // SizeByLife 모듈에서 trail 폭 배율로 소비하는 값
    EffectTrailSpawnPerUnitRuntimeDesc spawnPerUnit{}; // SpawnPerUnit 모듈에서 trail sample density를 보강하는 값

    float width{ 1.f };             // render sample의 base/tip span에 곱할 폭 비율. 1이면 원본 base/tip 폭을 유지
    float segmentLifetime{ 0.25f }; // history sample 하나가 화면에 남는 시간
    uint32 historyCount{ 32 };      // trail이 유지할 최대 sample 개수
    float uvTiling{ 1.f };          // trail noise/breakup 진행 방향의 기본 UV 반복값
    float maxTrailLength{ 0.f };    // head 기준 최대 표시 길. 0 이하면 제한하지 않음
    float tailFadeLength{ 0.1f };   // maxTrailLength 끝으로 갈수록 alpha를 줄이는 거리
    bool autoLifeFade{ true };      // sample age 기준 자동 alpha fade를 적용할지 결결정
    float sampleSpacing{ 0.08f };   // render-time curve sample의 이동 거리 기준 간격
    uint32 curveSubdivision{ 4 };   // 원본 sample interval 하나에서 만들 최대 render segment 수
    bool smoothTangent{ true };     // render sample 생성 전 원본 history tangent를 완만하게 보정할지 결결정
    float sideFade{ 0.08f };        // 폭 방향 alpha edge 감소 강도

    Weak<IEffectTrailSampleProvider> sampleProvider{}; // attachment 계산 owner가 공급하는 sample source
};

enum class EffectRibbonRenderAxis : uint8
{
    CameraUp,
    ViewUp,
    WorldUp,
    SourceUp,
    SourceRight,
};

enum class EffectRibbonSpreadBasis : uint8
{
    CameraFacing,
    ViewUp,
    WorldUp,
    SourceUp,
    SourceRight,
};

// Beam path를 어떤 authored 입력으로 만들지 결결정
enum class EffectBeamEndpointMode : uint8
{
    StartEnd,
    DirectionLength,
};

enum class EffectBeamBranchPreset : uint8
{
    EndGuided,
    DownStrike,
    Entangle,
    ShortCrack,
};

// BeamEnvelopeOverLife 모듈에서 beam visible interval과 width 배율을 평가하는 payload
struct EffectBeamEnvelopeOverLifeRuntimeDesc
{
    bool enabled{ false };
    float startRatioFallback{ 0.f };
    float endRatioFallback{ 1.f };
    float widthScaleFallback{ 1.f };
    PointParticleFloatCurveRuntimeDesc startRatioOverLife{};
    PointParticleFloatCurveRuntimeDesc endRatioOverLife{};
    PointParticleFloatCurveRuntimeDesc widthScaleOverLife{};
};

// generated beam이 local authored path를 camera-facing strip으로 낮추는 runtime payload
struct ComputeBeamEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };                 // Auto면 material family 기반 render group을 사용
    EffectRequiredMaterialRuntimeDesc material{};                 // beam 기본 material/tint/blend 값
    EffectEmitterPlaybackRuntimeDesc playback{};                  // duration/loop/delay 같은 공통 재생 규칙
    EffectLifetimeRuntimeDesc lifetime{};                         // beam presentation life 기준 값
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};            // Lifetime visual life random sample seed salt
    bool useLifetimeVisualLife{ false };                          // true면 Lifetime 모듈을 visual cutoff/over-life 기준으로 소비
    EffectRequiredSortRuntimeDesc sort{};                         // Required 모듈에서 Blend 정렬 규칙으로 소비
    EffectInitialColorRuntimeDesc initialColor{};                 // InitialColor 모듈에서 beam 시작 색으로 소비
    EffectColorOverLifeRuntimeDesc colorOverLife{};               // ColorOverLife 모듈에서 beam 색/알파로 소비
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};     // 수명 진행률 기준 SubUV frame 규칙
    PointParticleSizeByLifeDesc sizeByLife{};                     // SizeByLife 모듈에서 beam 폭 배율로 소비
    EffectBeamEnvelopeOverLifeRuntimeDesc beamEnvelopeOverLife{}; // BeamEnvelopeOverLife 모듈에서 visible interval/width 배율로 소비
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};    // SubUV RandomFrame replay 단위 seed salt
    bool usePerStripSubUVVariation{ false };                      // Legacy Beam strip별 SubUV RandomFrame variation compatibility flag

    EffectBeamEndpointMode endpointMode{
        EffectBeamEndpointMode::StartEnd };  // local path 작성 방식을 선택
    Vec3 localStart{ 0.f, 2.f, 0.f };        // effect/emitter local 기준 beam 시작점
    Vec3 localEnd{ 0.f, -2.f, 0.f };         // StartEnd 모드에서 쓰는 local 끝점
    Vec3 localDirection{ 0.f, -1.f, 0.f };   // DirectionLength 모드에서 쓰는 local 방향
    float length{ 4.f };                     // DirectionLength 모드에서 쓰는 beam 길
    uint32 segmentCount{ 8 };                // generated polyline segment 수
    float noiseAmplitude{ 0.f };             // 중심선에서 벗어나는 deterministic jitter 크기
    uint32 seed{ 1 };                        // generated path jitter seed
    uint32 stripCount{ 1 };                  // 같은 start에서 생성할 generated strip 수
    float endSpreadRadius{ 0.f };            // clustered end가 기준 end 주변에 퍼지는 반경
    float lengthVariance{ 0.f };             // strip별 길이 차이를 만드는 deterministic 편차
    EffectBeamBranchPreset branchPreset{
        EffectBeamBranchPreset::EndGuided }; // branch path 생성 preset
    bool branchEnabled{ false };             // main strip에서 짧은 child branch를 생성할지 여부
    uint32 branchCount{ 0 };                 // strip별 최대 branch 수
    float branchChance{ 1.f };               // branch 후보가 실제 생성될 확률
    uint32 branchSegmentCount{ 3 };          // child branch polyline segment 수
    float branchLength{ 0.75f };             // child branch 기본 길
    float branchLengthVariance{ 0.25f };     // branch별 길이 deterministic 편차
    float branchStartMin{ 0.2f };            // parent path에서 branch 시작 구간의 최소 비율
    float branchStartMax{ 0.85f };           // parent path에서 branch 시작 구간의 최대 비율
    float branchSpreadRadius{ 0.5f };        // branch end가 parent tangent에서 벗어나는 확산 반경
    float branchEndSpreadRadius{ 0.5f };     // EndGuided branch target이 beam end 주변에 퍼지는 반경
    float branchOutwardAmount{ 0.25f };      // EndGuided branch가 시작 직후 parent에서 벌어지는 정도
    float branchCurveAmount{ 0.5f };         // EndGuided branch가 target으로 수렴하는 휘어짐 정도
    float branchDownLength{ 1.f };           // DownStrike branch가 authored down axis로 뻗는 길
    float branchEntangleRadius{ 0.35f };     // Entangle branch가 parent 주변을 감는 반경
    float branchEntangleAdvance{ 0.25f };    // Entangle branch target이 parent path를 따라 이동하는 비율
    float branchCrackLength{ 0.5f };         // ShortCrack branch가 start 주변으로 짧게 뻗는 길
    float branchCrackSpreadRadius{ 0.25f };  // ShortCrack endpoint가 radial하게 퍼지는 반경
    float branchWidthScale{ 0.45f };         // parent 폭 대비 branch 폭 배율
    uint32 branchSeedOffset{ 1009 };         // parent seed와 섞어 branch 결과를 갈라놓는 salt
    float baseWidth{ 0.25f };                // SizeByLife를 곱하기 전 beam 기본 폭
    float tilingDistance{ 0.f };             // 0 이하면 visible length stretch, 양수면 거리 기준 texture 반복
};

enum class EffectSourceHistoryRibbonSourceMode : uint8
{
    SelfRoot,        // emitter root position을 source history로 사용
    ParticleEmitter, // 같은 EffectInstance 내부 particle lane position을 source history로 사용
};

// ParticleEmitter source mode에서 follower lane 하나가 참조할 source point
struct EffectFollowerSourcePoint
{
    uint32 sourceIndex{}; // source particle/lane 식별용 index
    Vec3 position{};      // 이번 frame에 기록할 source world position
};

// source/root position history를 camera-facing strip으로 낮추는 runtime payload
struct ComputeRibbonEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };              // Auto면 material family 기반 render group을 사용
    EffectRequiredMaterialRuntimeDesc material{};              // ribbon 기본 material/tint/blend 값
    EffectEmitterPlaybackRuntimeDesc playback{};               // duration/loop/delay 같은 공통 재생 규칙
    EffectLifetimeRuntimeDesc lifetime{};                      // Lifetime 모듈에서 sample age 기준 fallback으로 소비하는 값
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};         // Lifetime replay 단위 sample lifetime random sample seed salt
    bool useLifetimeSampleLifetime{ false };                   // true면 Lifetime 모듈을 sampleLifetime 기준으로 소비
    EffectRequiredSortRuntimeDesc sort{};                      // Required 모듈에서 Blend 정렬 규칙으로 소비하는 값
    EffectInitialColorRuntimeDesc initialColor{};              // InitialColor 모듈에서 source history sample 시작 색으로 소비하는 값
    EffectColorOverLifeRuntimeDesc colorOverLife{};            // ColorOverLife 모듈에서 sample age 기준 색/알파로 소비하는 값
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};  // 수명 진행률 기준 SubUV frame 규칙
    PointParticleSizeByLifeDesc sizeByLife{};                  // SizeByLife 모듈에서 source history strip 폭 배율로 소비하는 값
    PointParticleInitialLocationDesc headOffset{};             // InitialLocation 모듈에서 source history Head 위치 보정으로 소비하는 offset
    PointParticleRandomSeedRuntimeDesc headOffsetSeed{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{}; // SubUV RandomFrame replay 단위 seed salt

    EffectSourceHistoryRibbonSourceMode sourceMode{
        EffectSourceHistoryRibbonSourceMode::SelfRoot }; // source history sample을 만들 source owner 종류

    uint32 sourceEmitterId{};      // ParticleEmitter mode에서 같은 EffectInstance 안의 source emitter id
    uint32 followerLaneCount{ 1 }; // ParticleEmitter mode에서 동시에 따라갈 representative particle lane 상한
    uint32 maxSampleCount{ 64 };   // source history가 보관할 최대 sample 개수
    float sampleSpacing{ 0.08f };  // source가 이 거리 이상 움직였을 때 새 sample을 추가
    uint32 curveSubdivision{ 4 };  // source history segment 하나에서 만들 최대 곡선 render segment 수
    bool smoothTangent{ true };    // render sample 생성 전 source history path를 완만하게 보정할지 결결정
    float sampleInterval{ 0.f };   // 0보다 크면 정지/저속 source도 이 시간마다 sample을 보강. projectile tail 기본값은 0
    float sampleLifetime{ 0.35f }; // sample 하나가 strip 구성 후보로 남는 시간. 같은 시간 안에서 빠른 source일수록 tail이 길어짐

    float maxLength{ 0.f };      // head 기준 표시 가능한 최대 길. 목표 길이가 아니라 상한이며, 0 이하면 제한하지 않음
    float tailFadeLength{ 0.f }; // maxLength 끝으로 갈수록 alpha를 줄이는 거리. maxLength가 0이면 의미가 없음

    bool laneSpawnFadeInEnabled{ true };    // ParticleEmitter lane이 새로 생길 때 strip 폭 fade-in을 적용할지 결결정
    float laneSpawnFadeInDuration{ 0.08f }; // 새 follower lane 폭이 0에서 원래 폭으로 커지는 시간

    float baseWidth{ 0.3f };                     // SizeByLife를 곱하기 전 source history strip 기본 폭
    float tilingDistance{ 0.f };                 // 0 이하면 visible length stretch, 양수면 거리 기준 texture 반복
    bool autoLifeFade{ false };                  // sample age 기준 alpha fade를 적용해 기존 tail이 사라지는 과정을 보이게 할지 결결정
    bool tailCollapseOnIdle{ true };             // source가 느리거나 멈췄을 때 history sample을 source 쪽으로 당길지 결결정
    float tailCollapseSpeed{ 10.f };             // idle tail collapse 보간 속도
    EffectRibbonSpreadBasis spreadBasis{
        EffectRibbonSpreadBasis::CameraFacing }; // RibbonOrientation 모듈에서 선택한 폭 방향 기준축
    float spreadAngleDegrees{ 0.f };             // 선택 기준축에서 tangent 기준으로 추가 회전하는 펼침 각도
    bool useManualRoll{ false };                 // true면 shader에서 tangent 기준 ribbon 펼침 방향을 추가 회전
    float manualRollDegrees{ 0.f };              // useManualRoll일 때 적용할 tangent 기준 펼침 각도

    Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{}; // attachment 계산 owner가 공급하는 단일 source sample
};

enum class EffectSourceHistorySpriteTrailStampSpawnMode : uint8
{
    Distance,
    Time,
};

enum class EffectSourceHistorySpriteTrailDirectionMode : uint8
{
    PathTangent,
    CameraFacing,
    VelocityTextureAxis,
};

enum class EffectSourceHistorySpriteTrailPathFollowDirection : uint8
{
    TowardHead,
    TowardTail,
};

enum class EffectSourceHistorySpriteTrailPathReplayMode : uint8
{
    RecordedSpeed,
    FitDuration,
};

enum class EffectSourceHistorySpriteTrailPathReplayStartMode : uint8
{
    TailFirst,
    AllAtOnce,
};

// source history path 도착 후 stamp를 어떻게 처리할지 결정
enum class EffectSourceHistorySpriteTrailArrivalMode : uint8
{
    KillOnArrive,
    ClampAtEnd,
};

struct EffectSourceHistorySpriteTrailPathFollowRuntimeDesc
{
    bool enabled{ false };       // true면 stamp center를 local velocity가 아니라 source history path 좌표가 소유
    EffectSourceHistorySpriteTrailPathFollowDirection direction{ EffectSourceHistorySpriteTrailPathFollowDirection::TowardHead };
    Vec2 speed{ 1.f, 1.f };      // 초당 path distance 진행량 범위
    PointParticleRandomSeedRuntimeDesc speedSeed{};
    Vec2 startDelay{ 0.f, 0.f }; // stamp별 path-follow 시작 전 대기 시간 범위
    PointParticleRandomSeedRuntimeDesc startDelaySeed{};
    EffectSourceHistorySpriteTrailArrivalMode arrivalMode{ EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive };
};

struct EffectSourceHistorySpriteTrailPathReplayRuntimeDesc
{
    bool enabled{ false }; // true면 stamp center를 기록된 source history path 좌표 replay가 소유
    float delayTime{ 0.f };
    EffectSourceHistorySpriteTrailPathReplayMode replayMode{ EffectSourceHistorySpriteTrailPathReplayMode::RecordedSpeed };
    float speedScale{ 1.f };
    float drainDuration{ 0.35f };
    PointParticleFloatCurveRuntimeDesc drainCurve{};
    EffectSourceHistorySpriteTrailPathReplayStartMode startMode{ EffectSourceHistorySpriteTrailPathReplayStartMode::TailFirst };
    EffectSourceHistorySpriteTrailArrivalMode arrivalMode{ EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive };
};

// source/root history path 위에 Required sprite alignment 기반 sprite card stamp를 찍는 runtime payload
struct ComputeSourceHistorySpriteTrailEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{ EffectEmitterRenderLayerOverride::Auto };
    EffectRequiredMaterialRuntimeDesc material{};
    EffectEmitterPlaybackRuntimeDesc playback{};
    EffectDrawLimitRuntimeDesc drawLimit{};
    EffectLifetimeRuntimeDesc lifetime{};
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    bool useLifetimeStampLifetime{ false };
    EffectRequiredSortRuntimeDesc sort{};
    EffectInitialSizeRuntimeDesc initialSize{};
    EffectInitialColorRuntimeDesc initialColor{};
    EffectColorOverLifeRuntimeDesc colorOverLife{};
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{};
    PointParticleSizeByLifeDesc sizeByLife{};
    PointParticleMotionDesc motion{};
    PointParticleRotationDesc rotation{};
    PointParticleSpriteTiltDesc spriteTilt{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};
    EffectSpriteRenderRuntimeDesc spriteRender{};

    EffectSourceHistoryRibbonSourceMode sourceMode{ EffectSourceHistoryRibbonSourceMode::SelfRoot };
    uint32 sourceEmitterId{};
    uint32 followerLaneCount{ 1 };
    float sampleLifetime{ 0.35f };
    float sampleSpacing{ 0.08f };
    uint32 curveSubdivision{ 4 }; // source history segment 하나에서 만들 최대 곡선 sample 수
    bool smoothTangent{ true };   // path sample 생성 전 source history path를 완만하게 보정할지 결결정
    float maxLength{ 0.f };

    EffectSourceHistorySpriteTrailStampSpawnMode stampSpawnMode{ EffectSourceHistorySpriteTrailStampSpawnMode::Distance };
    float stampSpacing{ 0.12f };
    float stampInterval{ 0.03f };
    float stampLifetime{ 0.18f };
    uint32 maxStampCount{ 128 };
    float cardLength{ 0.45f };
    float cardWidth{ 0.08f };
    bool flipU{ false };
    bool flipV{ false };
    float rotationOffsetDegrees{ 0.f };
    float spawnJitter{ 0.f };
    EffectSourceHistorySpriteTrailPathFollowRuntimeDesc pathFollow{};
    EffectSourceHistorySpriteTrailPathReplayRuntimeDesc pathReplay{};

    Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{}; // attachment 계산 owner가 공급하는 단일 source sample
};

// mesh emitter가 Vec3 transform curve를 CPU instance update에서 평가하기 위한 payload
struct MeshVector3CurveRuntimeDesc
{
    bool enabled{ false };                      // true면 lifeProgress 기준 curve를 평가
    Vec3 start{ 1.f, 1.f, 1.f };                // distribution/legacy fallback 시작값
    Vec3 end{ 1.f, 1.f, 1.f };                  // distribution/legacy fallback 종료값
    uint32 curveKeyCount{ 2 };                  // compact curve key 수
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };   // key time 0..1 값
    Vec4 curveKeyTimesBlock1{};                 // key 4..7 time 값
    Vec4 curveKeyValuesX{ 1.f, 1.f, 0.f, 0.f }; // key별 X 값
    Vec4 curveKeyValuesXBlock1{};               // key 4..7 X 값
    Vec4 curveKeyValuesY{ 1.f, 1.f, 0.f, 0.f }; // key별 Y 값
    Vec4 curveKeyValuesYBlock1{};               // key 4..7 Y 값
    Vec4 curveKeyValuesZ{ 1.f, 1.f, 0.f, 0.f }; // key별 Z 값
    Vec4 curveKeyValuesZBlock1{};               // key 4..7 Z 값
    Vec4 curveKeyArriveTangentsX{};             // key별 X arrive tangent 값
    Vec4 curveKeyArriveTangentsXBlock1{};       // key 4..7 X arrive tangent 값
    Vec4 curveKeyLeaveTangentsX{};              // key별 X leave tangent 값
    Vec4 curveKeyLeaveTangentsXBlock1{};        // key 4..7 X leave tangent 값
    Vec4 curveKeyArriveTangentsY{};             // key별 Y arrive tangent 값
    Vec4 curveKeyArriveTangentsYBlock1{};       // key 4..7 Y arrive tangent 값
    Vec4 curveKeyLeaveTangentsY{};              // key별 Y leave tangent 값
    Vec4 curveKeyLeaveTangentsYBlock1{};        // key 4..7 Y leave tangent 값
    Vec4 curveKeyArriveTangentsZ{};             // key별 Z arrive tangent 값
    Vec4 curveKeyArriveTangentsZBlock1{};       // key 4..7 Z arrive tangent 값
    Vec4 curveKeyLeaveTangentsZ{};              // key별 Z leave tangent 값
    Vec4 curveKeyLeaveTangentsZBlock1{};        // key 4..7 Z leave tangent 값
    Vec4 curveKeyModes{ 1.f, 1.f, 0.f, 0.f };   // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 curveKeyModesBlock1{};                 // key 4..7 interpolation mode 값
};

// mesh emitter 전용 Vec3 scale/rotation runtime payload
struct MeshTransformRuntimeDesc
{
    EffectScreenAlignment alignment{ EffectScreenAlignment::FacingCameraPosition }; // mesh instance의 기본 plane/facing 정렬 기준

    bool initialScaleEnabled{ false };           // InitialMeshSize Vec3 payload 연결 여부
    Vec3 initialScaleMin{ 0.12f, 0.12f, 0.12f }; // spawn 시 mesh scale 하한
    Vec3 initialScaleMax{ 0.28f, 0.28f, 0.28f }; // spawn 시 mesh scale 상한
    PointParticleRandomSeedRuntimeDesc initialScaleSeed{};
    MeshVector3CurveRuntimeDesc scaleByLife{};   // MeshSizeByLife Vec3 multiplier

    bool rotationEnabled{ false };
    Vec3 initialRotationDegreesMin{ 0.f, 0.f, 0.f };          // spawn 시 Euler rotation 하한
    Vec3 initialRotationDegreesMax{ 0.f, 0.f, 0.f };          // spawn 시 Euler rotation 상한
    PointParticleRandomSeedRuntimeDesc initialRotationSeed{};
    MeshVector3CurveRuntimeDesc rotationByLife{};             // lifeProgress 기준 추가 Euler degree
    Vec3 initialAngularVelocityDegreesMin{ 0.f, 0.f, 0.f };   // 초당 Euler angular velocity 하한
    Vec3 initialAngularVelocityDegreesMax{ 0.f, 0.f, 0.f };   // 초당 Euler angular velocity 상한
    bool initialAngularVelocityInWorldSpace{ false };         // InitialMeshRotationRate 축을 world/simulation 기준으로 해석할지 여부
    PointParticleRandomSeedRuntimeDesc initialAngularVelocitySeed{};
    MeshVector3CurveRuntimeDesc angularVelocityScaleByLife{}; // angular velocity Vec3 배율
};

enum class EffectMeshDirectionAlignTargetMode : uint8
{
    Direction,
    Point,
};

enum class EffectMeshDirectionAlignSpace : uint8
{
    Local,
    World,
};

enum class EffectMeshDirectionAlignBlendMode : uint8
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

// MeshDirectionAlignOverLife 모듈이 mesh 최종 orientation에 적용하는 target-facing payload
struct MeshDirectionAlignRuntimeDesc
{
    bool enabled{ false };                                    // 모듈 연결 여부
    EffectMeshDirectionAlignTargetMode targetMode{ EffectMeshDirectionAlignTargetMode::Direction };
    EffectMeshDirectionAlignSpace space{ EffectMeshDirectionAlignSpace::World };
    Vec3 target{ 0.f, 0.f, 1.f };                             // Direction이면 벡터, Point면 위치
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ }; // target direction에 맞출 mesh local 축
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY }; // target frame 안정화에 사용할 mesh local up 축
    PointParticleFloatCurveRuntimeDesc alignmentProgress{};   // life progress 기준 0..1 정렬 weight curve
    Vec2 randomDelay{ 0.f, 0.f };                             // spawn 시 샘플되는 normalized life 지연 범위
    PointParticleRandomSeedRuntimeDesc randomDelaySeed{};
    Vec2 randomWeightScale{ 1.f, 1.f };                       // spawn 시 샘플되는 최종 weight 배율 범위
    PointParticleRandomSeedRuntimeDesc randomWeightScaleSeed{};
    EffectMeshDirectionAlignBlendMode blendMode{ EffectMeshDirectionAlignBlendMode::Linear };
};

// mesh emitter가 clone 시점에 받는 model/instance draw용 runtime payload
struct MeshEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto };                       // Auto면 material/blend mode 기반 render group을 사용
    string modelGuid{};                                                 // 선택 model asset identity
    string modelPath{};                                                 // asset-root 기준 model path fallback/debug hint
    Vec3 previewScale{ 1.f, 1.f, 1.f };                                 // Phase 3 단일 mesh particle preview scale
    bool useModelMaterials{ false };                                    // true면 source model material을 우선 사용
    EffectRequiredMaterialRuntimeDesc material{};                       // Phase 3에서는 tint/debug payload로 보존하고 material override는 후속에서 소비
    EffectEmitterPlaybackRuntimeDesc playback{};                        // Required 모듈에서 mesh emitter가 소비하는 재생 규칙
    EffectDrawLimitRuntimeDesc drawLimit{};                             // Required 모듈에서 mesh draw 제출 상한으로 소비하는 규칙
    EffectRequiredSortRuntimeDesc sort{};                               // Required 모듈에서 mesh emitter가 소비하는 Blend 정렬 규칙
    EffectSpawnRuntimeDesc spawn{};                                     // Spawn 모듈에서 mesh particle 활성화 규칙으로 소비하는 payload
    EffectLifetimeRuntimeDesc lifetime{};                               // Lifetime 모듈에서 mesh particle age/max life로 소비하는 payload
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    EffectInitialColorRuntimeDesc initialColor{};                       // InitialColor 모듈에서 mesh particle 시작 색으로 소비하는 payload
    EffectColorOverLifeRuntimeDesc colorOverLife{};                     // ColorOverLife 모듈에서 mesh particle color/fade로 소비하는 payload
    PointParticleMotionDesc motion{};                                   // InitialVelocity/RadialVelocity 모듈의 위치 갱신 payload
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};                     // OrbitOverLife 모듈에서 mesh particle center 위치 공전으로 소비하는 payload
    PointParticleInitialLocationDesc initialLocation{};                 // InitialLocation 모듈에서 mesh spawn offset으로 소비하는 payload
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};
    PointParticleSphereLocationDesc sphereLocation{};                   // SphereLocation 모듈에서 mesh spawn offset으로 소비하는 payload
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};         // PlaneRadialLocation 모듈에서 mesh spawn offset으로 소비하는 payload
    PointParticleCylinderLocationDesc cylinderLocation{};               // CylinderLocation 모듈에서 mesh spawn offset으로 소비하는 payload
    PointParticleSphereRadialOrientationDesc sphereRadialOrientation{}; // SphereRadialOrientation 모듈의 sphere radial frame 회전 payload
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{};   // PlaneRadialOrientation 모듈에서 mesh radial frame 회전으로 소비하는 payload
    PointParticleCylinderOrientationDesc cylinderOrientation{};         // CylinderOrientation 모듈에서 cylinder frame 회전으로 소비하는 payload
    MeshTransformRuntimeDesc meshTransform{};                           // Mesh 전용 Vec3 scale/rotation payload
    MeshDirectionAlignRuntimeDesc meshDirectionAlign{};                 // MeshDirectionAlignOverLife 모듈에서 최종 방향 정렬로 소비하는 payload
};

using EffectEmitterConcreteDesc = variant<
    ComputeSpriteEmitterDesc,
    ComputeTrailEmitterDesc,
    ComputeRibbonEmitterDesc,
    ComputeSourceHistorySpriteTrailEmitterDesc,
    ComputeBeamEmitterDesc,
    MeshEmitterDesc>;

struct EffectEmitterDefinition
{
    uint32 id{};                                               // authoring/runtime 추적용 emitter id
    string name{};                                             // 디버그/로그용 emitter 이름
    EffectEmitterKind kind{ EffectEmitterKind::Sprite };       // 생성할 concrete emitter 종류
    bool enabled{ true };                                      // root가 runtime child 생성 대상으로 볼지 결정하는 플래그
    EffectEmitterHistoryBudgetRuntimeDesc historyBudget{};     // Phase 1에서는 effective 표시/검증용이며 runtime density cap에는 쓰지 않음
    EffectHistoryBudgetEffectiveDesc effectiveHistoryBudget{}; // 현재 budget 입력에서 계산한 history 비용성 effective 값

    Vec3 localPosition{ 0.f, 0.f, 0.f };          // effect root 기준 emitter local 위치
    Vec3 localRotationDegrees{ 0.f, 0.f, 0.f };   // effect root 기준 emitter local 회전
    bool useLocalSpace{ false };                  // true면 root transform을 따라가는 local-space emitter
    EffectEmitterRenderLayerOverride renderLayerOverride{
        EffectEmitterRenderLayerOverride::Auto }; // Auto면 concrete emitter의 기존 자동 render group 정책을 따름

    EffectEmitterConcreteDesc concreteDesc{
        ComputeSpriteEmitterDesc{} }; // concrete emitter가 직접 소비할 payload
};

//## Effect::RuntimeDesc

// 저장/로드 결과로 공유되는 effect 정적 정의
struct EffectDefinition
{
    string name{};                                                    // effect asset/display 이름
    EffectHistoryBudgetRuntimeDesc historyBudget{};                   // effect 단위 history budget preset. Full 기본값은 기존 동작 보존
    vector<EffectHistorySourceGroupBudgetDesc> historySourceGroups{}; // effect 단위 source history group budget 정본
    vector<EffectEmitterDefinition> emitters{};                       // effect를 구성하는 emitter definition 목록
};

// EffectInstance root spawn 시 전달하는 runtime desc
struct EffectInstanceDesc : public GameObject::GAMEOBJECT_DESC
{
    Shared<const EffectDefinition> definition{}; // root가 재생할 정적 effect 정의
};

NS_END
