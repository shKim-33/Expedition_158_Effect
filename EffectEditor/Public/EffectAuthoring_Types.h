#pragma once
#include <variant>
#include "EffectMaterialAuthoring_Types.h"
#include "EffectRuntime_Types.h"
#include "Particle_Types.h"

NS_BEGIN(EffectEditor)

//## Types::AuthoringCore
enum class AuthoringModuleType : uint8
{
    Required,
    Spawn,
    Lifetime,
    InitialLocation,
    SphereLocation,
    PlaneRadialLocation,
    CylinderLocation,
    InitialSize,
    InitialMeshSize,
    InitialVelocity,
    InitialRadialVelocity,
    VelocityCone,
    SourceMotionVelocity,
    Acceleration,
    Drag,
    VelocityOverLife,
    OrbitOverLife,
    InitialRotation,
    SphereRadialOrientation,
    PlaneRadialOrientation,
    CylinderOrientation,
    RotationOverLife,
    SpriteTilt,
    SpriteTiltOverLife,
    InitialRotationRate,
    RotationRateScaleByLife,
    InitialMeshRotation,
    MeshRotationOverLife,
    MeshDirectionAlignOverLife,
    InitialMeshRotationRate,
    MeshRotationRateScaleByLife,
    InitialColor,
    ColorOverLife,
    SubUVFrameOverLife,
    SizeByLife,
    BeamEnvelopeOverLife,
    MeshSizeByLife,
    SpawnPerUnit,
    SourceHistorySpriteTrailPathFollow,
    SourceHistorySpriteTrailPathReplay,
    RibbonOrientation,
    MaterialScalarModulation,
};

enum class EffectAuthoringSelectionKind : uint8
{
    None,
    Emitter,
    TypeData,
    Module,
};

enum class EmitterRenderLayerOverride : uint8
{
    Auto,
    UIEffect,
};

//## Types::HistoryBudget
enum class HistoryBudgetPreset : uint8
{
    Full,
    High,
    Medium,
    Performance,
    Low,
};

enum class HistoryBudgetPriority : uint8
{
    Core,
    Secondary,
    Decorative,
    Distortion,
};

enum class HistoryBudgetDensityBias : uint8
{
    Dense,
    Normal,
    Sparse,
};

enum class HistoryBudgetUpdateRate : uint8
{
    EveryFrame,
    Every2Frames,
};

struct HistorySourceGroupBudgetData
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

struct HistoryBudgetData
{
    HistoryBudgetPreset preset{ HistoryBudgetPreset::Full };
    bool preserveLength{ true };
    bool sharedSourceHistory{ true };
    float trailDensityScale{ 1.f };
    float spriteStampDensityScale{ 1.f };
    float ribbonDensityScale{ 1.f };
    float distortionDensityScale{ 1.f };
    HistoryBudgetUpdateRate updateRate{ HistoryBudgetUpdateRate::EveryFrame };
    vector<HistorySourceGroupBudgetData> sourceGroups{};
};

struct EmitterHistoryBudgetData
{
    HistoryBudgetPriority priority{ HistoryBudgetPriority::Core };
    HistoryBudgetDensityBias densityBias{ HistoryBudgetDensityBias::Normal };
    float lengthScale{ 1.f };
};

//## Types::EmitterRender
enum class EmitterScreenAlignment : uint8
{
    Square,
    FacingCameraPosition,
    Rectangle,
    Velocity,
    AwayFromCenter,
    WorldPlaneXY,
    WorldPlaneXZ,
    TypeSpecific,
    FacingCameraDistanceBlend,
    WorldUpFacingCamera,
};

// AwayFromCenter/Velocity가 기준 방향을 어떻게 사용할지 결정
enum class EmitterDirectionalAlignmentMode : uint8
{
    LookDirection, // 기준 방향을 sprite 카드의 바라보는 방향으로 사용
    TextureAxis,   // camera-facing 평면은 유지하고 texture 축만 기준 방향에 정렬
};

// TextureAxis mode에서 기준 방향에 맞출 texture 축을 선택
enum class EmitterSpriteTextureAxis : uint8
{
    X, // texture의 가로 축을 radial/velocity 방향에 정렬
    Y, // texture의 세로 축을 radial/velocity 방향에 정렬
};

enum class EmitterSortMode : uint8
{
    None,
    ViewProjDepth,
    DistanceToView,
    AgeOldestFirst,
    AgeNewestFirst,
};

enum class EmitterSortPolicy : uint8
{
    None,
    EmitterDepth,
};

//## Distribution::Core
enum class DistributionValueKind : uint8
{
    Float,
    Vector2,
    Vector3,
    Color,
};

enum class DistributionMode : uint8
{
    Constant,
    Uniform,
    ConstantCurve,
    UniformCurve,
    ParticleParameter,
};

// 현재 authoring payload가 실제로 보유하는 분포 모드만 true로 유지
constexpr bool Is_DistributionModeEditable(DistributionMode mode)
{
    return mode == DistributionMode::Constant ||
           mode == DistributionMode::Uniform ||
           mode == DistributionMode::ConstantCurve;
}

enum class DistributionRandomSeedMode : uint8
{
    Default,
    Manual,
};

// 분포/random consumer 모듈이 같은 범위 안에서 재현 가능한 다른 draw를 만들기 위한 seed 설정
struct DistributionRandomSeedData
{
    DistributionRandomSeedMode mode{ DistributionRandomSeedMode::Default };
    uint32 manualSeed{ 0 };
    bool useInstanceSeed{ false };
};

// 분포 플로트에서의 보간 모드 설정
enum class FloatCurveInterpolationMode : uint8
{
    Linear,           // key 사이를 직선으로 잇는 기본 모드
    Constant,         // 다음 key 전까지 값을 유지하는 계단형 모드
    CurveAuto,        // 후속 지원 예정인 자동 tangent curve 모드
    CurveUser,        // 후속 지원 예정인 수동 tangent curve 모드
    CurveBreak,       // 후속 지원 예정인 좌우 tangent 분리 모드
    CurveAutoClamped, // 실제 지원할 안전한 자동 curve 모드
};

// 분포 플로트 상수 커브의 필요 값들
struct FloatCurveKeyData
{
    float time{ 0.f };                         // 0..1 공통축에서 이 key의 위치
    float value{ 0.f };                        // 이 key 시점의 출력값
    float arriveTangent{ 0.f };                // key로 들어오는 곡선 기울기
    float leaveTangent{ 0.f };                 // key에서 나가는 곡선 기울기
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear }; // 이 key 이후 구간의 보간 방식을 결결정
};

// 분포 플로트 상수 커브의 보간 key 목록을 보관
struct ConstantCurveFloatDistributionData
{
    // 분포 플로트 상수 커브의 authoring key 목록
    vector<FloatCurveKeyData> keys{
        FloatCurveKeyData{ 0.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
        FloatCurveKeyData{ 1.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
    };
};

// 분포 Vec2 상수 커브의 필요 값들
struct Vector2CurveKeyData
{
    float time{ 0.f };                         // 0..1 공통축에서 이 key의 위치
    Vec2 value{ 0.f, 0.f };                    // 이 key 시점의 X/Y 출력값
    Vec2 arriveTangent{};                      // key로 들어오는 X/Y 곡선 기울기
    Vec2 leaveTangent{};                       // key에서 나가는 X/Y 곡선 기울기
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear }; // 이 key 이후 구간의 보간 방식을 결결정
};

// 분포 Vec2 상수 커브의 보간 key 목록을 보관
struct ConstantCurveVector2DistributionData
{
    // 분포 Vec2 상수 커브의 authoring key 목록
    vector<Vector2CurveKeyData> keys{
        Vector2CurveKeyData{ 0.f, Vec2{ 0.f, 0.f }, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
        Vector2CurveKeyData{ 1.f, Vec2{ 0.f, 0.f }, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
    };
};

// 분포 Vec3 상수 커브의 필요 값들
struct Vector3CurveKeyData
{
    float time{ 0.f };                         // 0..1 공통축에서 이 key의 위치
    Vec3 value{ 0.f, 0.f, 0.f };               // 이 key 시점의 X/Y/Z 출력값
    Vec3 arriveTangent{};                      // key로 들어오는 X/Y/Z 곡선 기울기
    Vec3 leaveTangent{};                       // key에서 나가는 X/Y/Z 곡선 기울기
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear }; // 이 key 이후 구간의 보간 방식을 결결정
};

// 분포 Vec3 상수 커브의 보간 key 목록을 보관
struct ConstantCurveVector3DistributionData
{
    // 분포 Vec3 상수 커브의 authoring key 목록
    vector<Vector3CurveKeyData> keys{
        Vector3CurveKeyData{ 0.f, Vec3{ 0.f, 0.f, 0.f }, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
        Vector3CurveKeyData{ 1.f, Vec3{ 0.f, 0.f, 0.f }, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
    };
};

// Color 분포는 alpha를 별도 Float distribution에서 유지하므로 curve key의 w는 저장/편집하지 않음
struct ColorCurveKeyData
{
    float time{ 0.f };                         // 0..1 공통축에서 이 key의 위치
    Color value{ 1.f, 1.f, 1.f, 1.f };         // 이 key 시점의 RGB 출력값. alpha는 별도 분포가 정본
    Color arriveTangent{};                     // key로 들어오는 RGB 곡선 기울기
    Color leaveTangent{};                      // key에서 나가는 RGB 곡선 기울기
    FloatCurveInterpolationMode interpolationMode{
        FloatCurveInterpolationMode::Linear }; // 이 key 이후 구간의 보간 방식을 결결정
};

// Color RGB 상수 커브의 보간 key 목록을 보관
struct ConstantCurveColorDistributionData
{
    // Color RGB 상수 커브의 authoring key 목록
    vector<ColorCurveKeyData> keys{
        ColorCurveKeyData{ 0.f, Color{ 1.f, 1.f, 1.f, 1.f }, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
        ColorCurveKeyData{ 1.f, Color{ 1.f, 1.f, 1.f, 1.f }, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
    };
};

struct ConstantFloatDistributionData
{
    float value{ 0.f };
};

struct UniformFloatDistributionData
{
    float minValue{ 0.f };
    float maxValue{ 0.f };
    DistributionRandomSeedData randomSeed{};
};

struct FloatDistributionData
{
    using Payload = variant<
        ConstantFloatDistributionData,
        UniformFloatDistributionData,
        ConstantCurveFloatDistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantFloatDistributionData{} };

    static FloatDistributionData Make_Constant(float value)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantFloatDistributionData{ value };
        return data;
    }

    static FloatDistributionData Make_Uniform(float minValue, float maxValue)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformFloatDistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static FloatDistributionData Make_ConstantCurve(float value)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveFloatDistributionData{
            {
                FloatCurveKeyData{ 0.f, value, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
                FloatCurveKeyData{ 1.f, value, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static FloatDistributionData Make_ConstantCurve(float startValue, float endValue)
    {
        FloatDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveFloatDistributionData{
            {
                FloatCurveKeyData{ 0.f, startValue, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
                FloatCurveKeyData{ 1.f, endValue, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }
};

struct ConstantVector2DistributionData
{
    Vec2 value{ 0.f, 0.f };
};

struct UniformVector2DistributionData
{
    Vec2 minValue{ 0.f, 0.f };
    Vec2 maxValue{ 0.f, 0.f };
    DistributionRandomSeedData randomSeed{};
};

struct Vector2DistributionData
{
    using Payload = variant<
        ConstantVector2DistributionData,
        UniformVector2DistributionData,
        ConstantCurveVector2DistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantVector2DistributionData{} };

    static Vector2DistributionData Make_Constant(const Vec2& value)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantVector2DistributionData{ value };
        return data;
    }

    static Vector2DistributionData Make_Uniform(const Vec2& minValue, const Vec2& maxValue)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformVector2DistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static Vector2DistributionData Make_ConstantCurve(const Vec2& value)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector2DistributionData{
            {
                Vector2CurveKeyData{ 0.f, value, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
                Vector2CurveKeyData{ 1.f, value, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static Vector2DistributionData Make_ConstantCurve(const Vec2& startValue, const Vec2& endValue)
    {
        Vector2DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector2DistributionData{
            {
                Vector2CurveKeyData{ 0.f, startValue, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
                Vector2CurveKeyData{ 1.f, endValue, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }
};

struct ConstantVector3DistributionData
{
    Vec3 value{ 0.f, 0.f, 0.f };
};

struct UniformVector3DistributionData
{
    Vec3 minValue{ 0.f, 0.f, 0.f };
    Vec3 maxValue{ 0.f, 0.f, 0.f };
    DistributionRandomSeedData randomSeed{};
};

struct Vector3DistributionData
{
    using Payload = variant<
        ConstantVector3DistributionData,
        UniformVector3DistributionData,
        ConstantCurveVector3DistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantVector3DistributionData{} };

    static Vector3DistributionData Make_Constant(const Vec3& value)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantVector3DistributionData{ value };
        return data;
    }

    static Vector3DistributionData Make_Uniform(const Vec3& minValue, const Vec3& maxValue)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformVector3DistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static Vector3DistributionData Make_ConstantCurve(const Vec3& value)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector3DistributionData{
            {
                Vector3CurveKeyData{ 0.f, value, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
                Vector3CurveKeyData{ 1.f, value, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static Vector3DistributionData Make_ConstantCurve(const Vec3& startValue, const Vec3& endValue)
    {
        Vector3DistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveVector3DistributionData{
            {
                Vector3CurveKeyData{ 0.f, startValue, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
                Vector3CurveKeyData{ 1.f, endValue, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }
};

struct ConstantColorDistributionData
{
    Color value{ 1.f, 1.f, 1.f, 1.f };
};

struct UniformColorDistributionData
{
    Color minValue{ 1.f, 1.f, 1.f, 1.f };
    Color maxValue{ 1.f, 1.f, 1.f, 1.f };
    DistributionRandomSeedData randomSeed{};
};

struct ColorDistributionData
{
    using Payload = variant<
        ConstantColorDistributionData,
        UniformColorDistributionData,
        ConstantCurveColorDistributionData>;

    DistributionMode mode{ DistributionMode::Constant };
    Payload payload{ ConstantColorDistributionData{} };

    static ColorDistributionData Make_Constant(const Color& value)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::Constant;
        data.payload = ConstantColorDistributionData{ value };
        return data;
    }

    static ColorDistributionData Make_Uniform(const Color& minValue, const Color& maxValue)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::Uniform;
        data.payload = UniformColorDistributionData{ .minValue = minValue, .maxValue = maxValue };
        return data;
    }

    static ColorDistributionData Make_ConstantCurve(const Color& value)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveColorDistributionData{
            {
                ColorCurveKeyData{ 0.f, value, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
                ColorCurveKeyData{ 1.f, value, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }

    static ColorDistributionData Make_ConstantCurve(const Color& startValue, const Color& endValue)
    {
        ColorDistributionData data{};
        data.mode = DistributionMode::ConstantCurve;
        data.payload = ConstantCurveColorDistributionData{
            {
                ColorCurveKeyData{ 0.f, startValue, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
                ColorCurveKeyData{ 1.f, endValue, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
            }
        };
        return data;
    }
};

//## Types::ParticleSystem
struct ParticleSystemAuthoringData
{
    bool autoDeactivate{ true };
};

//## Emitter::TypeData
enum class AuthoringTypeDataKind : uint8
{
    None,
    Trail,
    Mesh,
    Ribbon,
    SourceHistorySpriteTrail,
    Beam,
};

// Beam path를 EffectEditor에서 어떤 방식으로 작성할지 결결정
enum class BeamEndpointMode : uint8
{
    StartEnd,
    DirectionLength,
};

enum class BeamBranchPreset : uint8
{
    EndGuided,
    DownStrike,
    Entangle,
    ShortCrack,
};

// AuthoredLocal single beam v0의 editor 저장 payload
struct BeamTypeData
{
    BeamEndpointMode endpointMode{ BeamEndpointMode::StartEnd };  // local path 작성 방식
    Vec3 localStart{ 0.f, 2.f, 0.f };                             // effect/emitter local 기준 시작점
    Vec3 localEnd{ 0.f, -2.f, 0.f };                              // StartEnd 모드의 끝점
    Vec3 localDirection{ 0.f, -1.f, 0.f };                        // DirectionLength 모드의 방향
    float length{ 4.f };                                          // DirectionLength 모드의 길
    uint32 segmentCount{ 8 };                                     // generated polyline segment 수
    float noiseAmplitude{ 0.f };                                  // deterministic path jitter 크기
    uint32 seed{ 1 };                                             // deterministic path jitter seed
    uint32 stripCount{ 1 };                                       // 같은 start에서 생성할 strip 수
    float endSpreadRadius{ 0.f };                                 // clustered end가 기준 end 주변에 퍼지는 반경
    float lengthVariance{ 0.f };                                  // strip별 길이 차이를 만드는 deterministic 편차
    BeamBranchPreset branchPreset{ BeamBranchPreset::EndGuided }; // branch path 생성 preset
    bool branchEnabled{ false };                                  // main strip에서 짧은 child branch를 생성할지 여부
    uint32 branchCount{ 0 };                                      // strip별 최대 branch 수
    float branchChance{ 1.f };                                    // branch 후보가 실제 생성될 확률
    uint32 branchSegmentCount{ 3 };                               // child branch polyline segment 수
    float branchLength{ 0.75f };                                  // child branch 기본 길
    float branchLengthVariance{ 0.25f };                          // branch별 길이 deterministic 편차
    float branchStartMin{ 0.2f };                                 // parent path에서 branch 시작 구간의 최소 비율
    float branchStartMax{ 0.85f };                                // parent path에서 branch 시작 구간의 최대 비율
    float branchSpreadRadius{ 0.5f };                             // branch end가 parent tangent에서 벗어나는 확산 반경
    float branchEndSpreadRadius{ 0.5f };                          // EndGuided branch target이 beam end 주변에 퍼지는 반경
    float branchOutwardAmount{ 0.25f };                           // EndGuided branch가 시작 직후 parent에서 벌어지는 정도
    float branchCurveAmount{ 0.5f };                              // EndGuided branch가 target으로 수렴하는 휘어짐 정도
    float branchDownLength{ 1.f };                                // DownStrike branch가 authored down axis로 뻗는 길
    float branchEntangleRadius{ 0.35f };                          // Entangle branch가 parent 주변을 감는 반경
    float branchEntangleAdvance{ 0.25f };                         // Entangle branch target이 parent path를 따라 이동하는 비율
    float branchCrackLength{ 0.5f };                              // ShortCrack branch가 start 주변으로 짧게 뻗는 길
    float branchCrackSpreadRadius{ 0.25f };                       // ShortCrack endpoint가 radial하게 퍼지는 반경
    float branchWidthScale{ 0.45f };                              // parent 폭 대비 branch 폭 배율
    uint32 branchSeedOffset{ 1009 };                              // parent seed와 섞어 branch 결과를 갈라놓는 salt
    float baseWidth{ 0.25f };                                     // SizeByLife 전 기본 폭
    float tilingDistance{ 0.f };                                  // 0 이하면 전체 길이 stretch, 양수면 거리 기준 반복
};

enum class TrailCurveQuality : uint8
{
    Basic,       // 기존 호환에 가까운 기본 trail 곡선 preset
    Smooth,      // 보간 밀도를 높여 swing arc를 부드럽게 만드는 preset
    HighQuality, // 더 촘촘한 sample로 곡선을 만드는 고품질 preset
    Custom,      // 세부 곡선 값을 직접 편집하는 모드
};

enum class RibbonCurveQuality : uint8
{
    Basic,       // 낮은 보간 비용의 기본 source history sample preset
    Smooth,      // 더 촘촘한 sample로 follower ribbon 곡선을 부드럽게 생성
    HighQuality, // 가장 촘촘한 sample preset
    Custom,      // 세부 sample 값을 직접 편집하는 모드
};

struct EmptyTypeData
{};

enum class SourceHistoryRibbonSourceMode : uint8
{
    SelfRoot,
    ParticleEmitter,
    SourceEmitter, // Legacy root follower token. Save/Open에서 ParticleEmitter 또는 SelfRoot로 정리
};

enum class SourceHistorySpriteTrailStampSpawnMode : uint8
{
    Distance,
    Time,
};

enum class SourceHistorySpriteTrailDirectionMode : uint8
{
    PathTangent,
    CameraFacing,
    VelocityTextureAxis,
};

// Source History Ribbon이 어떤 source position history를 따라갈지 결정하는 editor 저장 payload
struct RibbonTypeData
{
    SourceHistoryRibbonSourceMode sourceMode{
        SourceHistoryRibbonSourceMode::SelfRoot };                // SelfRoot면 emitter root, ParticleEmitter면 다른 particle lane을 source로 사용
    uint32 sourceEmitterId{};                                     // ParticleEmitter mode에서 따라갈 source emitter id
    uint32 followerLaneCount{ 1 };                                // ParticleEmitter mode에서 동시에 추적할 representative lane 상한
    RibbonCurveQuality curveQuality{ RibbonCurveQuality::Basic }; // sampleSpacing/sampleInterval preset 선택값
    uint32 maxSampleCount{ 64 };                                  // lane 하나가 보관할 source history sample 상한
    float sampleSpacing{ 0.08f };                                 // source가 이 거리 이상 움직이면 새 history sample을 추가
    uint32 curveSubdivision{ 4 };                                 // source history segment 하나에서 만들 최대 곡선 render sample 수
    bool smoothTangent{ false };                                  // Custom에서 명시적으로 켤 때만 앞뒤 source sample 평균으로 path를 보결정
    float sampleInterval{ 0.f };                                  // 0보다 크면 정지/저속 source도 시간 기준 sample을 보강
    float maxLength{ 0.f };                                       // head 기준 최대 표시 길. 목표 길이가 아니라 상한
    float tailFadeLength{ 0.f };                                  // maxLength 끝부분에서 거리 기반 alpha fade를 적용하는 길
    bool laneSpawnFadeInEnabled{ true };                          // 새 follower lane 시작 폭을 짧게 fade-in할지 결결정
    float laneSpawnFadeInDuration{ 0.08f };                       // 새 follower lane 폭 fade-in 시간
    float baseWidth{ 0.3f };                                      // SizeByLife를 곱하기 전 source history strip 기본 폭
    float tilingDistance{ 0.f };                                  // 0 이하면 표시 길이에 stretch, 양수면 거리 기준 texture 반복
    bool autoLifeFade{ false };                                   // sample age 기준 alpha fade를 적용해 기존 tail이 사라지는 과정을 보이게 할지 결결정
    bool tailCollapseOnIdle{ true };                              // source가 느리거나 멈췄을 때 history sample을 source 쪽으로 당길지 결결정
    float tailCollapseSpeed{ 10.f };                              // idle tail collapse 보간 속도
    bool useManualRoll{ false };                                  // true면 tangent 기준으로 ribbon 펼침 방향을 추가 회전
    float manualRollDegrees{ 0.f };                               // useManualRoll일 때 적용할 tangent 기준 펼침 각도
};

// Source history path 위에 짧은 sprite/card stamp를 찍는 editor 저장 payload
struct SourceHistorySpriteTrailTypeData
{
    SourceHistoryRibbonSourceMode sourceMode{ SourceHistoryRibbonSourceMode::SelfRoot };
    uint32 sourceEmitterId{};
    uint32 followerLaneCount{ 1 };
    float sampleLifetime{ 0.35f };
    RibbonCurveQuality curveQuality{ RibbonCurveQuality::Basic };
    float sampleSpacing{ 0.08f };
    uint32 curveSubdivision{ 4 };
    bool smoothTangent{ true };
    float maxLength{ 0.f };
    SourceHistorySpriteTrailStampSpawnMode stampSpawnMode{ SourceHistorySpriteTrailStampSpawnMode::Distance };
    float stampSpacing{ 0.12f };
    float stampInterval{ 0.03f };
    uint32 maxStampCount{ 128 };
    float cardLength{ 0.45f };
    float cardWidth{ 0.08f };
    SourceHistorySpriteTrailDirectionMode directionMode{ SourceHistorySpriteTrailDirectionMode::PathTangent };
    EffectSpriteTextureAxis textureAxis{ EffectSpriteTextureAxis::Y };
    bool flipU{ false };
    bool flipV{ false };
    float rotationOffsetDegrees{ 0.f };
    float spawnJitter{ 0.f };
};

struct TrailTypeData
{
    float width{ 1.f };
    float segmentLifetime{ 0.25f };
    uint32 historyCount{ 32 };
    float uvTiling{ 1.f };          // body/mask가 아니라 noise/breakup 길이 방향 반복 밀도
    float maxTrailLength{ 0.f };    // 0 이하면 head 기준 최대 표시 길이를 제한하지 않음
    float tailFadeLength{ 0.1f };   // maxTrailLength 끝으로 갈수록 alpha를 줄이는 거리
    bool autoLifeFade{ true };      // sample age 기준 자동 alpha fade를 적용할지 결결정
    TrailCurveQuality curveQuality{
        TrailCurveQuality::Basic }; // trail 곡선 품질 preset 선택값
    float sampleSpacing{ 0.08f };   // 이동 거리 기준 중간 sample 삽입 간격
    uint32 curveSubdivision{ 4 };   // 한 frame에서 보강할 최대 중간 sample 수
    bool smoothTangent{ true };     // 앞뒤 sample 평균으로 곡선을 완만하게 보정할지 결결정
    float sideFade{ 0.08f };        // 폭 방향 alpha edge 감소 강도
};

struct MeshTypeData
{
    string modelGuid{};
    string modelPath{};
    string assignedEffectMaterialGuid{};
    string assignedEffectMaterialPath{};
    bool hasAssignedMaterialInstance{ false };
    EffectMaterialInstanceData assignedMaterial{};
    Vec3 previewScale{ 1.f, 1.f, 1.f };
    bool useModelMaterials{ false };
};

using AuthoringTypeDataPayload = variant<
    EmptyTypeData,
    TrailTypeData,
    MeshTypeData,
    RibbonTypeData,
    SourceHistorySpriteTrailTypeData,
    BeamTypeData>;

struct AuthoringTypeData
{
    AuthoringTypeDataKind kind{ AuthoringTypeDataKind::None };
    AuthoringTypeDataPayload payload{ EmptyTypeData{} };
};

//## Module::RequiredSpawnLifetime
struct RequiredModuleData
{
    string rendererType{ "Sprite" };
    EffectMaterialInstanceData material{};

    Vec3 emitterOrigin{ 0.f, 0.f, 0.f };
    Vec3 emitterRotationDegrees{ 0.f, 0.f, 0.f };

    EmitterScreenAlignment screenAlignment{
        EmitterScreenAlignment::FacingCameraPosition };
    EmitterDirectionalAlignmentMode directionalAlignmentMode{
        EmitterDirectionalAlignmentMode::TextureAxis }; // AwayFromCenter/Velocity에서 기준 방향을 look으로 쓸지 texture 축으로 쓸지 결결정
    EmitterSpriteTextureAxis spriteTextureAxis{
        EmitterSpriteTextureAxis::Y };                  // TextureAxis mode에서 기준 방향에 맞출 texture 축
    float spriteRollOffsetDegrees{ 0.f };               // texture authored orientation 보정용 정적 roll offset

    float minCameraBlendDistance{ 0.f };
    float maxCameraBlendDistance{ 0.f };
    bool useLocalSpace{ true };
    EmitterSortPolicy sortPolicy{ EmitterSortPolicy::None };
    int32 sortLayer{ 0 };
    EmitterSortMode sortMode{ EmitterSortMode::None };
    float sortBias{ 0.f };
    float cameraMotionBlurAmount{ 0.f };
    bool clearExistingParticlesOnInit{ false };
    uint32 loopCount{ 0 }; // Cascade: Emitter 루프 횟수. 0 은 무한 루프
    float duration{ 1.f };
    float durationLow{ 0.f };
    bool useDurationRange{ false };
    bool recalculateDurationEachLoop{ false };
    float delay{ 0.f };
    float delayLow{ 0.f };
    bool useDelayRange{ false };
    bool delayFirstLoopOnly{ false };
    bool killOnDeactivate{ false };
    bool killOnCompleted{ false };
    bool useMaxDrawCount{ true };
    uint32 maxDrawCount{ 500 };
};

struct SpawnModuleData
{
    struct ParticleBurstData
    {
        float time{ 0.f };
        uint32 count{ 16 };
    };

    bool processSpawnRate{ true };
    FloatDistributionData spawnRate{ FloatDistributionData::Make_Constant(10.f) };
    FloatDistributionData spawnRateScale{ FloatDistributionData::Make_Constant(1.f) };
    bool processBurstList{ true };
    vector<ParticleBurstData> burstList{ ParticleBurstData{} };
    FloatDistributionData burstScale{ FloatDistributionData::Make_Constant(1.f) };
    uint32 maxParticleCount{ 64 };
};

struct LifetimeModuleData
{
    FloatDistributionData lifeTime{ FloatDistributionData::Make_Uniform(0.5f, 1.f) };
};

//## Module::LocationAndOrientation
struct InitialLocationModuleData
{
    Vector3DistributionData location{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
};

enum class SphereLocationSpawnMode : uint8
{
    Volume,
    Surface,
};

enum class SphereLocationPlacementMode : uint8
{
    Random,
    EvenByParticleIndex,
};

struct SphereLocationModuleData
{
    Vec3 offset{ 0.f, 0.f, 0.f };
    float radius{ 1.f };
    SphereLocationSpawnMode spawnMode{ SphereLocationSpawnMode::Volume };
    SphereLocationPlacementMode placementMode{ SphereLocationPlacementMode::Random };
    DistributionRandomSeedData randomSeed{};
};

enum class PlaneRadialLocationPlane : uint8
{
    XY,
    XZ,
    YZ,
    CameraFacing,
};

enum class PlaneRadialLocationShape : uint8
{
    Rectangle,
    Disc,
    Ring,
    Arc,
};

enum class PlaneRadialLocationPlacementMode : uint8
{
    Random,
    EvenByParticleIndex,
};

struct PlaneRadialLocationModuleData
{
    PlaneRadialLocationPlane plane{ PlaneRadialLocationPlane::XY };
    PlaneRadialLocationShape shape{ PlaneRadialLocationShape::Disc };
    PlaneRadialLocationPlacementMode placementMode{ PlaneRadialLocationPlacementMode::Random };
    Vec3 offset{ 0.f, 0.f, 0.f };
    FloatDistributionData uDistribution{ FloatDistributionData::Make_Uniform(-0.5f, 0.5f) };
    FloatDistributionData vDistribution{ FloatDistributionData::Make_Uniform(-0.5f, 0.5f) };
    FloatDistributionData radiusDistribution{ FloatDistributionData::Make_Uniform(0.f, 1.f) };
    FloatDistributionData angleDegreesDistribution{ FloatDistributionData::Make_Uniform(0.f, 360.f) };
    float thickness{ 0.f };
};

enum class CylinderLocationAxis : uint8
{
    LocalX,
    LocalY,
    LocalLook,
};

enum class CylinderLocationSpawnMode : uint8
{
    SideSurface,
    Volume,
};

enum class CylinderLocationPlacementMode : uint8
{
    Random,
    EvenByParticleIndex,
};

struct CylinderLocationModuleData
{
    CylinderLocationAxis axis{ CylinderLocationAxis::LocalLook };
    CylinderLocationSpawnMode spawnMode{ CylinderLocationSpawnMode::SideSurface };
    CylinderLocationPlacementMode placementMode{ CylinderLocationPlacementMode::Random };
    Vec3 offset{ 0.f, 0.f, 0.f };
    FloatDistributionData radiusDistribution{ FloatDistributionData::Make_Uniform(0.f, 1.f) };
    FloatDistributionData heightDistribution{ FloatDistributionData::Make_Uniform(-0.5f, 0.5f) };
    FloatDistributionData angleDegreesDistribution{ FloatDistributionData::Make_Uniform(0.f, 360.f) };
};

enum class PlaneRadialOrientationTargetKind : uint8
{
    Auto,
    Sprite2D,
    Mesh3D,
};

enum class PlaneRadialOrientationMode : uint8
{
    None,
    FaceRadialOut,
    FaceRadialIn,
    FaceTangentCW,
    FaceTangentCCW,
    FacePlaneNormal,
};

enum class PlaneRadialOrientationAxis : uint8
{
    PositiveX,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ,
};

struct PlaneRadialOrientationModuleData
{
    PlaneRadialOrientationTargetKind targetKind{ PlaneRadialOrientationTargetKind::Auto };
    PlaneRadialOrientationMode orientationMode{ PlaneRadialOrientationMode::None };
    PlaneRadialOrientationAxis meshForwardAxis{ PlaneRadialOrientationAxis::PositiveZ };
    PlaneRadialOrientationAxis meshUpAxis{ PlaneRadialOrientationAxis::PositiveY };
    float tiltDegrees{ 0.f };
    float rollOffsetDegrees{ 0.f };
};

enum class CylinderOrientationMode : uint8
{
    None,
    FaceRadialOut,
    FaceRadialIn,
    FaceTangentCW,
    FaceTangentCCW,
    FaceCylinderAxisPositive,
    FaceCylinderAxisNegative,
};

struct CylinderOrientationModuleData
{
    PlaneRadialOrientationTargetKind targetKind{ PlaneRadialOrientationTargetKind::Auto };
    CylinderOrientationMode orientationMode{ CylinderOrientationMode::None };
    bool followOrbitOverLife{ false };
    PlaneRadialOrientationAxis meshForwardAxis{ PlaneRadialOrientationAxis::PositiveZ };
    PlaneRadialOrientationAxis meshUpAxis{ PlaneRadialOrientationAxis::PositiveY };
    float tiltDegrees{ 0.f };
    float rollOffsetDegrees{ 0.f };
};

enum class SphereRadialOrientationMode : uint8
{
    None,
    FaceRadialOut,
    FaceRadialIn,
};

struct SphereRadialOrientationModuleData
{
    SphereRadialOrientationMode orientationMode{ SphereRadialOrientationMode::None };
    PlaneRadialOrientationAxis meshForwardAxis{ PlaneRadialOrientationAxis::PositiveZ };
    PlaneRadialOrientationAxis meshUpAxis{ PlaneRadialOrientationAxis::PositiveY };
    float tiltDegrees{ 0.f };
    float rollOffsetDegrees{ 0.f };
};

enum class RibbonSpreadBasis : uint8
{
    CameraFacing,
    ViewUp,
    WorldUp,
    SourceUp,
    SourceRight,
};

struct RibbonOrientationModuleData
{
    RibbonSpreadBasis spreadBasis{ RibbonSpreadBasis::CameraFacing };
    float spreadAngleDegrees{ 0.f };
};

//## Module::SizeAndMotion
struct InitialSizeModuleData
{
    Vector2DistributionData size{ Vector2DistributionData::Make_Constant(Vec2{ 0.3f, 0.3f }) };
};

struct InitialMeshSizeModuleData
{
    Vector3DistributionData size{ Vector3DistributionData::Make_Constant(Vec3{ 0.1f, 0.1f, 0.1f }) };
};

struct InitialVelocityModuleData
{
    Vector3DistributionData velocity{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
    bool inWorldSpace{ false };
};

enum class InitialRadialVelocityCenterDirectionMode : uint8
{
    RandomUpward,
    PlaneRadial,
};

struct InitialRadialVelocityModuleData
{
    FloatDistributionData speed{ FloatDistributionData::Make_Uniform(1.f, 2.f) };
    Vec3 radialPivot{ 0.f, 0.f, 0.f };
    InitialRadialVelocityCenterDirectionMode centerDirectionMode{ InitialRadialVelocityCenterDirectionMode::RandomUpward };
    bool inWorldSpace{ false };
};

struct VelocityConeModuleData
{
    Vec3 axis{ 0.f, 1.f, 0.f };
    float angleDegrees{ 30.f };
    FloatDistributionData speed{ FloatDistributionData::Make_Uniform(1.f, 2.f) };
    bool inWorldSpace{ false };
};

enum class SourceMotionVelocityDirectionMode : uint8
{
    InheritSourceVelocity,
    SourceVelocityDirection,
    SourceVelocityOpposite,
    TrailTangent,
    TrailTangentOpposite,
    SideFromTangent,
    RandomSideFromTangent,
};

struct SourceMotionVelocityModuleData
{
    SourceMotionVelocityDirectionMode directionMode{ SourceMotionVelocityDirectionMode::TrailTangent };
    FloatDistributionData speed{ FloatDistributionData::Make_Constant(0.f) };
    float sourceSpeedScale{ 0.f };
    float spreadAngleDegrees{ 0.f };
};

enum class AccelerationTimeBasis : uint8
{
    ParticleLife,
    EmitterNormalizedTime,
};

struct AccelerationModuleData
{
    Vector3DistributionData acceleration{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
    AccelerationTimeBasis timeBasis{ AccelerationTimeBasis::ParticleLife };
    bool inWorldSpace{ true };
};

struct DragModuleData
{
    FloatDistributionData drag{ FloatDistributionData::Make_Constant(0.f) };
};

enum class VelocityOverLifeApplyChannel : uint8
{
    InitialVelocity,
    InitialRadialVelocity,
    VelocityCone,
    SourceMotionVelocity,
    AccelerationIntegratedVelocity,
};

inline constexpr uint32 kVelocityOverLifeApplyChannelInitialVelocityMask{ 1u << 0 };
inline constexpr uint32 kVelocityOverLifeApplyChannelInitialRadialVelocityMask{ 1u << 1 };
inline constexpr uint32 kVelocityOverLifeApplyChannelVelocityConeMask{ 1u << 2 };
inline constexpr uint32 kVelocityOverLifeApplyChannelSourceMotionVelocityMask{ 1u << 3 };
inline constexpr uint32 kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask{ 1u << 4 };
inline constexpr uint32 kVelocityOverLifeApplyChannelAllMask{
    kVelocityOverLifeApplyChannelInitialVelocityMask |
    kVelocityOverLifeApplyChannelInitialRadialVelocityMask |
    kVelocityOverLifeApplyChannelVelocityConeMask |
    kVelocityOverLifeApplyChannelSourceMotionVelocityMask |
    kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask
};
inline constexpr uint32 kVelocityOverLifeApplyChannelDefaultMask{
    kVelocityOverLifeApplyChannelAllMask & ~kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask
};

struct VelocityOverLifeModuleData
{
    FloatDistributionData scaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f, 1.f) };
    uint32 applyChannelMask{ kVelocityOverLifeApplyChannelDefaultMask };
};

//## Module::OrbitAndRotation
enum class EffectOrbitPivotMode : uint8
{
    EmitterOrigin,
    SpawnOrigin,
};

enum class EffectOrbitPlane : uint8
{
    XY,
    XZ,
    YZ,
};

enum class EffectOrbitOrientationMode : uint8
{
    Preserve,
    InheritOrbitAngle,
    FaceRadialOut,
    FaceTangent,
};

struct OrbitOverLifeModuleData
{
    EffectOrbitPivotMode pivotMode{ EffectOrbitPivotMode::EmitterOrigin };
    EffectOrbitPlane plane{ EffectOrbitPlane::XY };
    FloatDistributionData angleDegreesOverLife{ FloatDistributionData::Make_ConstantCurve(0.f, 0.f) };
    FloatDistributionData radiusScaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f, 1.f) };
    EffectOrbitOrientationMode orientationMode{ EffectOrbitOrientationMode::Preserve };
};

struct InitialRotationModuleData
{
    FloatDistributionData rotationDegrees{ FloatDistributionData::Make_Constant(0.f) };
};

struct RotationOverLifeModuleData
{
    FloatDistributionData rotationOverLife{ FloatDistributionData::Make_ConstantCurve(0.f, 0.f) };
};

struct SpriteTiltModuleData
{
    Vector2DistributionData tiltDegrees{ Vector2DistributionData::Make_Constant(Vec2{ 0.f, 0.f }) };
};

struct SpriteTiltOverLifeModuleData
{
    Vector2DistributionData tiltOverLife{ Vector2DistributionData::Make_ConstantCurve(Vec2{ 0.f, 0.f }) };
};

struct InitialRotationRateModuleData
{
    FloatDistributionData rotationRateDegrees{ FloatDistributionData::Make_Constant(0.f) };
};

struct RotationRateScaleByLifeModuleData
{
    FloatDistributionData scaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f, 1.f) };
};

struct InitialMeshRotationModuleData
{
    Vector3DistributionData rotationDegrees{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
};

struct MeshRotationOverLifeModuleData
{
    Vector3DistributionData rotationOverLife{ Vector3DistributionData::Make_ConstantCurve(Vec3{ 0.f, 0.f, 0.f }) };
};

enum class MeshDirectionAlignTargetMode : uint8
{
    Direction,
    Point,
};

enum class MeshDirectionAlignSpace : uint8
{
    Local,
    World,
};

enum class MeshDirectionAlignBlendMode : uint8
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

struct MeshDirectionAlignOverLifeModuleData
{
    MeshDirectionAlignTargetMode targetMode{ MeshDirectionAlignTargetMode::Direction };
    MeshDirectionAlignSpace space{ MeshDirectionAlignSpace::World };
    Vec3 target{ 0.f, 0.f, 1.f };
    PlaneRadialOrientationAxis meshForwardAxis{ PlaneRadialOrientationAxis::PositiveZ };
    PlaneRadialOrientationAxis meshUpAxis{ PlaneRadialOrientationAxis::PositiveY };
    FloatDistributionData alignmentProgress{ FloatDistributionData::Make_ConstantCurve(0.f, 0.f) };
    FloatDistributionData randomDelay{ FloatDistributionData::Make_Constant(0.f) };
    FloatDistributionData randomWeightScale{ FloatDistributionData::Make_Constant(1.f) };
    MeshDirectionAlignBlendMode blendMode{ MeshDirectionAlignBlendMode::Linear };
};

struct InitialMeshRotationRateModuleData
{
    Vector3DistributionData rotationRateDegrees{ Vector3DistributionData::Make_Constant(Vec3{ 0.f, 0.f, 0.f }) };
    bool inWorldSpace{ false };
};

struct MeshRotationRateScaleByLifeModuleData
{
    Vector3DistributionData scaleOverLife{ Vector3DistributionData::Make_ConstantCurve(Vec3{ 1.f, 1.f, 1.f }) };
};

//## Module::ColorSubUVAndSize
struct InitialColorModuleData
{
    // 색상 RGB와 별도로 생성 시점 alpha를 편집하기 위한 축
    ColorDistributionData color{ ColorDistributionData::Make_Constant(Color{ 1.f, 1.f, 1.f, 1.f }) };
    FloatDistributionData alpha{ FloatDistributionData::Make_Constant(1.f) };
};

struct ColorOverLifeModuleData
{
    // 색상 변화와 독립적으로 fade in/out을 잡기 위한 수명 기준 alpha 축
    ColorDistributionData colorOverLife{ ColorDistributionData::Make_Constant(Color{ 1.f, 1.f, 1.f, 1.f }) };
    FloatDistributionData alphaOverLife{ FloatDistributionData::Make_Constant(1.f) };
};

struct SubUVFrameOverLifeModuleData
{
    FloatDistributionData frameIndex{ FloatDistributionData::Make_ConstantCurve(0.f) };
    uint32 startFrame{ 0 };
    uint32 endFrame{ 0 };
    bool loop{ false };
    SubUVFramePlaybackMode playbackMode{ SubUVFramePlaybackMode::FixedFrame };
    float framesPerSecond{ 12.f };
    bool randomStartPhase{ false };
    DistributionRandomSeedData randomSeed{};
    bool perStripSubUVVariation{ false }; // Legacy Beam multi-strip SubUV variation field
};

struct SizeByLifeModuleData
{
    enum class AxisLock : uint8
    {
        None,
        X,
        Y,
    };

    Vector2DistributionData scaleOverLife{ Vector2DistributionData::Make_ConstantCurve(Vec2{ 1.f, 1.f }) };
    bool multiplyX{ true };
    bool multiplyY{ true };
    AxisLock axisLock{ AxisLock::None };
};

struct BeamEnvelopeOverLifeModuleData
{
    FloatDistributionData startRatioOverLife{ FloatDistributionData::Make_ConstantCurve(0.f) };
    FloatDistributionData endRatioOverLife{ FloatDistributionData::Make_ConstantCurve(1.f) };
    FloatDistributionData widthScaleOverLife{ FloatDistributionData::Make_ConstantCurve(1.f) };
};

struct MeshSizeByLifeModuleData
{
    Vector3DistributionData scaleOverLife{ Vector3DistributionData::Make_ConstantCurve(Vec3{ 1.f, 1.f, 1.f }) };
    bool multiplyX{ true };
    bool multiplyY{ true };
    bool multiplyZ{ true };
};

struct SpawnPerUnitModuleData
{
    FloatDistributionData spawnPerUnit{ FloatDistributionData::Make_Constant(50.f) };
    float unitScalar{ 1.f };
    float movementTolerance{ 0.001f };
    float maxFrameDistance{ 0.f };
};

//## Module::SourceHistoryMotion
enum class SourceHistorySpriteTrailPathFollowDirection : uint8
{
    TowardHead,
    TowardTail,
};

enum class SourceHistorySpriteTrailArrivalMode : uint8
{
    KillOnArrive,
    ClampAtEnd,
};

struct SourceHistorySpriteTrailPathFollowModuleData
{
    SourceHistorySpriteTrailPathFollowDirection direction{ SourceHistorySpriteTrailPathFollowDirection::TowardHead };
    FloatDistributionData speed{ FloatDistributionData::Make_Constant(1.f) };
    FloatDistributionData startDelay{ FloatDistributionData::Make_Constant(0.f) };
    SourceHistorySpriteTrailArrivalMode arrivalMode{ SourceHistorySpriteTrailArrivalMode::KillOnArrive };
};

enum class SourceHistorySpriteTrailPathReplayMode : uint8
{
    RecordedSpeed,
    FitDuration,
};

enum class SourceHistorySpriteTrailPathReplayStartMode : uint8
{
    TailFirst,
    AllAtOnce,
};

struct SourceHistorySpriteTrailPathReplayModuleData
{
    float delayTime{ 0.f };
    SourceHistorySpriteTrailPathReplayMode replayMode{ SourceHistorySpriteTrailPathReplayMode::RecordedSpeed };
    float speedScale{ 1.f };
    float drainDuration{ 0.35f };
    FloatDistributionData drainCurve{ FloatDistributionData::Make_ConstantCurve(0.f, 1.f) };
    SourceHistorySpriteTrailPathReplayStartMode startMode{ SourceHistorySpriteTrailPathReplayStartMode::TailFirst };
    SourceHistorySpriteTrailArrivalMode arrivalMode{ SourceHistorySpriteTrailArrivalMode::KillOnArrive };
};

//## Module::MaterialModulation
enum class MaterialScalarModulationTargetField : uint8
{
    Intensity,
    OpacityPower,
    NoiseStrength,
    AlphaErosion,
    AlphaCutoff,
    CoreIntensity,
    OuterIntensity,
    CoreColor,
    RefractionIntensity,
    MainUVScrollSpeedScale,
    NoiseUVScrollSpeedScale,
    MaskUVScrollSpeedScale,
    FlowUVScrollSpeedScale,
    AlphaMultiplier,
};

enum class MaterialVec2ModulationTargetField : uint8
{
    MainUVOffset,
    NoiseUVOffset,
    MaskUVOffset,
    FlowUVOffset,
};

enum class MaterialScalarModulationOperation : uint8
{
    Multiply,
};

enum class MaterialScalarModulationTimeSource : uint8
{
    ParticleLife,
    EmitterTime,
};

struct MaterialScalarModulatorData
{
    bool enabled{ true };
    MaterialScalarModulationTargetField targetField{ MaterialScalarModulationTargetField::Intensity };
    MaterialScalarModulationOperation operation{ MaterialScalarModulationOperation::Multiply };
    MaterialScalarModulationTimeSource timeSource{ MaterialScalarModulationTimeSource::EmitterTime };
    FloatDistributionData distribution{ FloatDistributionData::Make_Constant(1.f) };
};

struct MaterialCoreColorRgbModulatorData
{
    bool enabled{ true };
    MaterialScalarModulationTimeSource timeSource{ MaterialScalarModulationTimeSource::EmitterTime };
    Vector3DistributionData distribution{ Vector3DistributionData::Make_Constant(Vec3{ 1.f, 0.85f, 0.45f }) };
};

struct MaterialVec2ModulatorData
{
    bool enabled{ true };
    MaterialVec2ModulationTargetField targetField{ MaterialVec2ModulationTargetField::MainUVOffset };
    MaterialScalarModulationTimeSource timeSource{ MaterialScalarModulationTimeSource::EmitterTime };
    Vector2DistributionData distribution{ Vector2DistributionData::Make_Constant(Vec2{ 0.f, 0.f }) };
};

struct MaterialScalarModulationModuleData
{
    vector<MaterialScalarModulatorData> modulators{};
    vector<MaterialCoreColorRgbModulatorData> coreColorRgbModulators{};
    vector<MaterialVec2ModulatorData> vec2Modulators{};
};

using AuthoringModuleData = variant<
    RequiredModuleData,
    SpawnModuleData,
    LifetimeModuleData,
    InitialLocationModuleData,
    SphereLocationModuleData,
    PlaneRadialLocationModuleData,
    CylinderLocationModuleData,
    InitialSizeModuleData,
    InitialMeshSizeModuleData,
    InitialVelocityModuleData,
    InitialRadialVelocityModuleData,
    VelocityConeModuleData,
    SourceMotionVelocityModuleData,
    AccelerationModuleData,
    DragModuleData,
    VelocityOverLifeModuleData,
    OrbitOverLifeModuleData,
    InitialRotationModuleData,
    SphereRadialOrientationModuleData,
    PlaneRadialOrientationModuleData,
    CylinderOrientationModuleData,
    RotationOverLifeModuleData,
    SpriteTiltModuleData,
    SpriteTiltOverLifeModuleData,
    InitialRotationRateModuleData,
    RotationRateScaleByLifeModuleData,
    InitialMeshRotationModuleData,
    MeshRotationOverLifeModuleData,
    MeshDirectionAlignOverLifeModuleData,
    InitialMeshRotationRateModuleData,
    MeshRotationRateScaleByLifeModuleData,
    InitialColorModuleData,
    ColorOverLifeModuleData,
    SubUVFrameOverLifeModuleData,
    SizeByLifeModuleData,
    BeamEnvelopeOverLifeModuleData,
    MeshSizeByLifeModuleData,
    SpawnPerUnitModuleData,
    RibbonOrientationModuleData,
    std::monostate, // Temporary ReSharper C++ workaround
    SourceHistorySpriteTrailPathFollowModuleData,
    SourceHistorySpriteTrailPathReplayModuleData,
    MaterialScalarModulationModuleData>;

//## Types::AuthoringAggregate
struct AuthoringModule
{
    uint32 id{};
    AuthoringModuleType type{ AuthoringModuleType::Required };
    string key{};
    wstring displayName{};
    wstring summary{};
    bool enabled{ true };
    bool movable{ true };
    bool removable{ true };
    AuthoringModuleData data{ RequiredModuleData{} };
};

struct AuthoringEmitter
{
    uint32 id{};
    string name{};
    string rendererType{ "Sprite" };
    EmitterRenderLayerOverride renderLayerOverride{ EmitterRenderLayerOverride::Auto };
    EmitterHistoryBudgetData historyBudget{};
    string resourceSummary{};
    string textureId{ "Effects/Textures/Shared/DefaultTexture.dds" };
    bool enabled{ true };
    bool previewDirty{ true };
    AuthoringTypeData typeData{};
    vector<AuthoringModule> modules{};
};

struct EffectAuthoringSelection
{
    EffectAuthoringSelectionKind kind{ EffectAuthoringSelectionKind::None };
    uint32 emitterId{};
    uint32 moduleId{};
};

struct EffectAuthoringDocument
{
    string name{};
    ParticleSystemAuthoringData particleSystemData{};
    HistoryBudgetData historyBudget{};
    vector<AuthoringEmitter> emitters{};
};

NS_END
