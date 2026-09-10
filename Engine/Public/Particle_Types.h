#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

inline constexpr uint32 kEffectDistributionCurveMaxKeys{ 8 };

// point particle 검증 경로에서 사용할 instanced draw 제출 방식
enum class PointParticleDrawMode : uint8
{
    Direct,                      // 기존 DrawInstanced 경로로 제출
    DrawInstancedIndirect,       // CPU가 작성한 args buffer로 DrawInstancedIndirect를 호출
    DrawIndexedInstancedIndirect // CPU가 작성한 args buffer로 DrawIndexedInstancedIndirect를 호출
};

// SubUV frame 선택/재생 기본 방식을 결결정
enum class SubUVFramePlaybackMode : uint8
{
    FixedFrame,
    LifeProgress,
    FramesPerSecond,
    RandomFrame,
};

// GPU instance buffer에 저장되는 per-instance world/lifetime/color payload
struct ParticleInstanceVertex
{
    Vec4 right{};                                // 인스턴스 월드 행렬의 right 행
    Vec4 up{};                                   // 인스턴스 월드 행렬의 up 행
    Vec4 look{};                                 // compute sprite에서는 xyz를 velocity alignment hint로도 사용
    Vec4 translation{};                          // 인스턴스 월드 행렬의 위치 행
    Vec2 lifeTime{};                             // x: 최대 생존 시간, y: 누적 생존 시간
    Vec4 startColor{ 1.f, 1.f, 1.f, 1.f };       // particle 생성 시점의 색상/알파
    Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };         // particle 수명 종료 시점의 색상/알파
    Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f };        // x/y는 uv 시작점, z/w는 uv 끝점
    Vec2 spriteTiltDegrees{};                    // x/y: roll 적용 이후 card local X/Y 기준 tilt degree
    Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f }; // Core Color RGB Uniform+ParticleLife 생성 시점 샘플
};

// Particle shader가 사용하는 vertex + instance input layout
struct ParticleInstanceLayout
{
    static constexpr uint32 numElements = 11;

    // WORLD 0~3은 semantic name WORLD + semantic index 0~3으로 표현
    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 128, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

struct TrailInstanceVertex
{
    Vec4 right{};         // segment 진행 방향과 길이를 담는 instance row
    Vec4 up{};            // base-tip 폭 방향과 길이를 담는 instance row
    Vec4 look{};          // v0에서는 camera-facing 보정 없이 예비 축으로 유지
    Vec4 translation{};   // segment 중심 world position
    Vec2 lifeTime{};      // x: segmentLifetime, y: age.
    Vec4 startColor{};    // segment 시작 색상
    Vec4 endColor{};      // segment 끝 색상
    Vec4 subUVRect{};     // segment별 trail UV 영역
    Vec4 segmentParams{}; // x: age 진행률, y: width 배율, z: alpha 배율, w: reserved
    Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
};

// Trail shader가 사용하는 vertex + instance input layout
struct TrailInstanceLayout
{
    static constexpr uint32 numElements = 11;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 136, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

struct EffectMeshInstanceLayout
{
    static constexpr uint32 numElements = 12;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "INST_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_LIFE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_CORE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// Source-history ribbon shader가 사용하는 vertex + instance input layout
// ComputeSourceHistoryRibbonEmitter::RibbonInstanceVertex와 byte layout을 일치
struct RibbonInstanceLayout
{
    static constexpr uint32 numElements = 11; // ComputeSourceHistoryRibbonEmitter::RibbonInstanceVertex와 byte layout을 일치

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 136, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// Generated beam shader가 사용하는 vertex + instance input layout
// ComputeGeneratedBeamEmitter::BeamInstanceVertex와 byte layout을 일치
struct BeamInstanceLayout
{
    static constexpr uint32 numElements = 11;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 120, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 136, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// Source-history sprite trail shader가 사용하는 vertex + instance input layout
// ComputeSourceHistorySpriteTrailEmitter::StampInstanceVertex와 byte layout을 일치
struct SourceHistorySpriteTrailInstanceLayout
{
    static constexpr uint32 numElements = 9;

    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 40, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 56, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 104, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// SubUVFrameOverLife 모듈의 최소 preview runtime desc
struct PointParticleSubUVFrameOverLifeDesc
{
    bool enabled{ false };                    // SubUV frame over life 모듈이 실제로 켜져 있는지 여부
    uint32 startFrame{ 0 };                   // FixedFrame에서는 선택 frame, 재생 모드에서는 시작 frame
    uint32 endFrame{ 0 };                     // 재생 모드에서 종료 frame
    bool loop{ false };                       // FPS 재생에서 frame range를 반복할지 여부
    SubUVFramePlaybackMode playbackMode{
        SubUVFramePlaybackMode::FixedFrame }; // 고정 선택, 수명 맞춤, FPS 재생, 랜덤 선택 중 하나를 결결정
    float framesPerSecond{ 12.f };            // FPS 재생 모드에서 초당 넘길 frame 수
    bool randomStartPhase{ false };           // FPS 재생에서 particle/stamp/strip별 시작 phase를 다르게 할지 여부
    uint32 frameCurveKeyCount{ 1 };           // 후속 Curve Editor 이관 후보인 compact frame curve key 수
    // frameCurve* : 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키). Values = frame index
    Vec4 frameCurveTimes{};
    Vec4 frameCurveTimesBlock1{};
    Vec4 frameCurveValues{};
    Vec4 frameCurveValuesBlock1{};
};

// Float ConstantCurve distribution을 runtime에서 평가하기 위한 compact payload
struct PointParticleFloatCurveRuntimeDesc
{
    bool enabled{ false };     // true면 sampling event phase 기준 curve를 평가
    uint32 curveKeyCount{ 2 }; // compact curve key 수

#pragma region curveKey*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyTimesBlock1{};
    Vec4 curveKeyValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyValuesBlock1{};
    Vec4 curveKeyArriveTangents{};
    Vec4 curveKeyArriveTangentsBlock1{};
    Vec4 curveKeyLeaveTangents{};
    Vec4 curveKeyLeaveTangentsBlock1{};
    Vec4 curveKeyModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 curveKeyModesBlock1{};
#pragma endregion
};

enum class PointParticleSizeByLifeAxisLock : uint8
{
    None,
    X,
    Y,
};

// SizeByLife 모듈의 최소 preview runtime desc
struct PointParticleSizeByLifeDesc
{
    bool enabled{ false };                      // Size By Life 모듈이 실제로 켜져 있는지 여부
    float multiplyXStart{ 1.f };                // legacy endpoint fallback: 수명 시작 시 X축 배율
    float multiplyXEnd{ 1.f };                  // legacy endpoint fallback: 수명 종료 시 X축 배율
    float multiplyYStart{ 1.f };                // legacy endpoint fallback: 수명 시작 시 Y축 배율
    float multiplyYEnd{ 1.f };                  // legacy endpoint fallback: 수명 종료 시 Y축 배율
    bool multiplyX{ true };                     // false면 평가된 X 대신 1을 사용
    bool multiplyY{ true };                     // false면 평가된 Y 대신 1을 사용
    PointParticleSizeByLifeAxisLock axisLock{}; // Sprite 2D 범위의 X/Y 축 잠금 정책
    uint32 curveKeyCount{ 2 };                  // scaleOverLife compact curve key 수

#pragma region curveKey* (size by life)
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키). ValuesX / ValuesY = X / Y scale multiplier
    Vec4 curveKeyTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyTimesBlock1{};
    Vec4 curveKeyValuesX{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyValuesXBlock1{};
    Vec4 curveKeyValuesY{ 1.f, 1.f, 0.f, 0.f };
    Vec4 curveKeyValuesYBlock1{};
    Vec4 curveKeyArriveTangentsX{};
    Vec4 curveKeyArriveTangentsXBlock1{};
    Vec4 curveKeyLeaveTangentsX{};
    Vec4 curveKeyLeaveTangentsXBlock1{};
    Vec4 curveKeyArriveTangentsY{};
    Vec4 curveKeyArriveTangentsYBlock1{};
    Vec4 curveKeyLeaveTangentsY{};
    Vec4 curveKeyLeaveTangentsYBlock1{};
    Vec4 curveKeyModes{ 1.f, 1.f, 0.f, 0.f };   // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 curveKeyModesBlock1{};
#pragma endregion
};

// 모듈별 random consumer가 lifecycle seed에 더할 수동 seed salt
struct PointParticleRandomSeedRuntimeDesc
{
    bool manualSeedEnabled{ false }; // true면 seed를 해당 consumer random sample에 가산
    uint32 seed{ 0 };                // 모듈 authoring에서 저장된 수동 seed 값. 0도 유효 seed
    bool useInstanceSeed{ false };   // true면 effect 재생 1회 seed를 해당 consumer random sample에 가산
};

// SpriteTilt 계열 모듈이 카드 면 기울기를 평가하기 위한 runtime payload
struct PointParticleSpriteTiltDesc
{
    bool initialTiltEnabled{ false };
    Vec2 tiltDegreesMin{ 0.f, 0.f };                       // spawn/stamp 시점 X/Y tilt 하한
    Vec2 tiltDegreesMax{ 0.f, 0.f };                       // spawn/stamp 시점 X/Y tilt 상한
    PointParticleRandomSeedRuntimeDesc tiltSeed{};
    bool tiltOverLifeEnabled{ false };
    bool tiltOverLifeCurveEnabled{ false };                // true면 over-life 값을 compact curve로 평가
    Vec2 tiltOverLifeMin{ 0.f, 0.f };                      // over-life fallback 시작 X/Y tilt
    Vec2 tiltOverLifeMax{ 0.f, 0.f };                      // over-life fallback 종료 X/Y tilt
    PointParticleRandomSeedRuntimeDesc tiltOverLifeSeed{}; // Uniform over-life offset seed salt
    uint32 tiltOverLifeCurveKeyCount{ 2 };                 // tiltOverLife compact curve key 수

#pragma region tiltOverLifeCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
    Vec4 tiltOverLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 tiltOverLifeCurveTimesBlock1{};
    Vec4 tiltOverLifeCurveValuesX{ 0.f, 0.f, 0.f, 0.f };
    Vec4 tiltOverLifeCurveValuesXBlock1{};
    Vec4 tiltOverLifeCurveValuesY{ 0.f, 0.f, 0.f, 0.f };
    Vec4 tiltOverLifeCurveValuesYBlock1{};
    Vec4 tiltOverLifeCurveArriveTangentsX{};
    Vec4 tiltOverLifeCurveArriveTangentsXBlock1{};
    Vec4 tiltOverLifeCurveLeaveTangentsX{};
    Vec4 tiltOverLifeCurveLeaveTangentsXBlock1{};
    Vec4 tiltOverLifeCurveArriveTangentsY{};
    Vec4 tiltOverLifeCurveArriveTangentsYBlock1{};
    Vec4 tiltOverLifeCurveLeaveTangentsY{};
    Vec4 tiltOverLifeCurveLeaveTangentsYBlock1{};
    Vec4 tiltOverLifeCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 tiltOverLifeCurveModesBlock1{};
#pragma endregion
};

// ColorOverLife 모듈의 ConstantCurve payload를 compute shader가 평가할 수 있게 압축한 desc
struct PointParticleColorOverLifeCurveDesc
{
    bool colorCurveEnabled{ false }; // true면 RGB를 lifeProgress 기준 curve로 평가
    bool alphaCurveEnabled{ false }; // true면 alpha를 lifeProgress 기준 curve로 평가
    uint32 colorCurveKeyCount{ 2 };  // RGB compact curve key 수
    uint32 alphaCurveKeyCount{ 2 };  // alpha compact curve key 수

#pragma region alphaCurve* / colorCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
    Vec4 colorCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveTimesBlock1{};
    Vec4 colorCurveValuesR{ 1.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveValuesRBlock1{};
    Vec4 colorCurveValuesG{ 1.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveValuesGBlock1{};
    Vec4 colorCurveValuesB{ 1.f, 1.f, 0.f, 0.f };
    Vec4 colorCurveValuesBBlock1{};
    Vec4 colorCurveArriveR{};
    Vec4 colorCurveArriveRBlock1{};
    Vec4 colorCurveArriveG{};
    Vec4 colorCurveArriveGBlock1{};
    Vec4 colorCurveArriveB{};
    Vec4 colorCurveArriveBBlock1{};
    Vec4 colorCurveLeaveR{};
    Vec4 colorCurveLeaveRBlock1{};
    Vec4 colorCurveLeaveG{};
    Vec4 colorCurveLeaveGBlock1{};
    Vec4 colorCurveLeaveB{};
    Vec4 colorCurveLeaveBBlock1{};
    Vec4 colorCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 colorCurveModesBlock1{};
    Vec4 alphaCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 alphaCurveTimesBlock1{};
    Vec4 alphaCurveValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 alphaCurveValuesBlock1{};
    Vec4 alphaCurveArrive{};
    Vec4 alphaCurveArriveBlock1{};
    Vec4 alphaCurveLeave{};
    Vec4 alphaCurveLeaveBlock1{};
    Vec4 alphaCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 alphaCurveModesBlock1{};
#pragma endregion
};

// InitialRadialVelocity가 중심에서 방향을 만들 수 없을 때 사용할 방향 정책
enum class PointParticleInitialRadialVelocityCenterDirectionMode : uint32
{
    RandomUpward = 0,
    PlaneRadial = 1,
};

enum class PointParticleSourceMotionVelocityDirectionMode : uint32
{
    InheritSourceVelocity = 0,
    SourceVelocityDirection = 1,
    SourceVelocityOpposite = 2,
    TrailTangent = 3,
    TrailTangentOpposite = 4,
    SideFromTangent = 5,
    RandomSideFromTangent = 6,
};

enum class PointParticleAccelerationTimeBasis : uint32
{
    ParticleLife = 0,
    EmitterNormalizedTime = 1,
};

enum class PointParticleVelocityScaleChannel : uint32
{
    InitialVelocity = 0,
    InitialRadialVelocity = 1,
    VelocityCone = 2,
    SourceMotionVelocity = 3,
    AccelerationIntegratedVelocity = 4,
};

// motion 계열 모듈을 compute particle update가 바로 소비할 수 있게 압축한 payload
struct PointParticleMotionDesc
{
    // *InWorldSpace : true 면 emitter basis 변환을 건너뛰고 world 값으로 해석
    bool enabled{ false };                               // motion 계열 모듈이 하나라도 preview에 연결되었는지 여부
    bool initialVelocityEnabled{ false };
    bool initialVelocityInWorldSpace{ false };
    bool initialRadialVelocityEnabled{ false };
    bool initialRadialVelocityInWorldSpace{ false };     // true면 radial pivot을 world offset으로 해석
    bool velocityConeEnabled{ false };
    bool velocityConeInWorldSpace{ false };
    bool sourceMotionVelocityEnabled{ false };
    bool accelerationInWorldSpace{ true };
    Vec3 initialVelocityMin{ 0.f, 0.f, 0.f };
    Vec3 initialVelocityMax{ 0.f, 0.f, 0.f };
    Vec3 radialPivot{ 0.f, 0.f, 0.f };                   // InitialRadialVelocity의 방사 방향 기준점
    Vec2 radialSpeed{ 0.f, 0.f };                        // InitialRadialVelocity speed 분포의 min/max
    Vec3 velocityConeAxis{ 0.f, 1.f, 0.f };              // VelocityCone의 중심축
    float velocityConeAngleDegrees{ 30.f };              // VelocityCone의 중심축 기준 half-angle
    Vec2 velocityConeSpeed{ 0.f, 0.f };                  // VelocityCone speed 분포의 min/max
    PointParticleSourceMotionVelocityDirectionMode sourceMotionVelocityDirectionMode{
        PointParticleSourceMotionVelocityDirectionMode::TrailTangent
    };                                                   // SourceMotionVelocity가 source motion에서 방향을 고르는 정책
    Vec2 sourceMotionVelocitySpeed{ 0.f, 0.f };          // SourceMotionVelocity speed 분포의 min/max
    float sourceMotionVelocitySourceSpeedScale{ 0.f };   // source speed를 시작 속도 크기에 더할 배율
    float sourceMotionVelocitySpreadAngleDegrees{ 0.f }; // source motion 방향 주변 local 분산 half-angle
    PointParticleInitialRadialVelocityCenterDirectionMode initialRadialVelocityCenterDirectionMode{
        PointParticleInitialRadialVelocityCenterDirectionMode::RandomUpward
    };                                                   // 중심 중첩 시 InitialRadialVelocity가 사용할 방향 정책
    Vec3 accelerationMin{ 0.f, 0.f, 0.f };
    Vec3 accelerationMax{ 0.f, 0.f, 0.f };
    PointParticleAccelerationTimeBasis accelerationTimeBasis{
        PointParticleAccelerationTimeBasis::ParticleLife
    };                                                   // Acceleration ConstantCurve를 평가할 시간 기준
    bool accelerationCurveEnabled{ false };              // true면 Acceleration ConstantCurve를 선택된 시간 기준으로 절대 가속 평가
    uint32 accelerationCurveKeyCount{ 2 };               // Acceleration compact curve key 수

#pragma region accelerationCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
    Vec4 accelerationCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 accelerationCurveTimesBlock1{};
    Vec4 accelerationCurveValuesX{ 0.f, 0.f, 0.f, 0.f };
    Vec4 accelerationCurveValuesXBlock1{};
    Vec4 accelerationCurveValuesY{ 0.f, 0.f, 0.f, 0.f };
    Vec4 accelerationCurveValuesYBlock1{};
    Vec4 accelerationCurveValuesZ{ 0.f, 0.f, 0.f, 0.f };
    Vec4 accelerationCurveValuesZBlock1{};
    Vec4 accelerationCurveArriveTangentsX{};
    Vec4 accelerationCurveArriveTangentsXBlock1{};
    Vec4 accelerationCurveLeaveTangentsX{};
    Vec4 accelerationCurveLeaveTangentsXBlock1{};
    Vec4 accelerationCurveArriveTangentsY{};
    Vec4 accelerationCurveArriveTangentsYBlock1{};
    Vec4 accelerationCurveLeaveTangentsY{};
    Vec4 accelerationCurveLeaveTangentsYBlock1{};
    Vec4 accelerationCurveArriveTangentsZ{};
    Vec4 accelerationCurveArriveTangentsZBlock1{};
    Vec4 accelerationCurveLeaveTangentsZ{};
    Vec4 accelerationCurveLeaveTangentsZBlock1{};
    Vec4 accelerationCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 accelerationCurveModesBlock1{};
#pragma endregion
    Vec2 drag{ 0.f, 0.f };                             // Drag 분포의 min/max 감쇠값
    Vec2 velocityScaleByLife{ 1.f, 1.f };              // VelocityOverLife의 시작/종료 fallback 배율
    uint32 velocityScaleByLifeCurveKeyCount{ 2 };      // VelocityOverLife compact curve key 수

#pragma region velocityScaleByLifeCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
    Vec4 velocityScaleByLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 velocityScaleByLifeCurveTimesBlock1{};
    Vec4 velocityScaleByLifeCurveValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 velocityScaleByLifeCurveValuesBlock1{};
    Vec4 velocityScaleByLifeCurveArriveTangents{};
    Vec4 velocityScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 velocityScaleByLifeCurveLeaveTangents{};
    Vec4 velocityScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 velocityScaleByLifeCurveModes{ 1.f, 1.f, 0.f, 0.f };                       // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 velocityScaleByLifeCurveModesBlock1{};
#pragma endregion
    PointParticleRandomSeedRuntimeDesc initialVelocitySeed{};
    PointParticleRandomSeedRuntimeDesc initialRadialVelocitySeed{};
    PointParticleRandomSeedRuntimeDesc velocityConeSeed{};
    PointParticleRandomSeedRuntimeDesc sourceMotionVelocitySeed{};                  // SourceMotionVelocity speed/spread seed salt
    PointParticleRandomSeedRuntimeDesc accelerationSeed{};
    PointParticleRandomSeedRuntimeDesc dragSeed{};
    PointParticleFloatCurveRuntimeDesc initialVelocityScaleByLife{};                // InitialVelocity channel 전용 VelocityOverLife 배율
    PointParticleFloatCurveRuntimeDesc initialRadialVelocityScaleByLife{};          // InitialRadialVelocity channel 전용 VelocityOverLife 배율
    PointParticleFloatCurveRuntimeDesc velocityConeScaleByLife{};                   // VelocityCone channel 전용 VelocityOverLife 배율
    PointParticleFloatCurveRuntimeDesc sourceMotionVelocityScaleByLife{};           // SourceMotionVelocity channel 전용 VelocityOverLife 배율
    PointParticleFloatCurveRuntimeDesc accelerationIntegratedVelocityScaleByLife{}; // Acceleration 누적 속도 channel 전용 VelocityOverLife 배율
};

// rotation 계열 모듈을 compute particle update가 바로 소비할 수 있게 압축한 payload
struct PointParticleRotationDesc
{
    bool enabled{ false };                       // rotation 계열 모듈이 하나라도 preview에 연결되었는지 여부
    Vec2 initialRotationDegrees{ 0.f, 0.f };     // InitialRotation 분포의 min/max 각도
    Vec2 rotationOverLifeDegrees{ 0.f, 0.f };    // RotationOverLife curve의 시작/종료 fallback 추가 각도
    Vec2 initialRotationRateDegrees{ 0.f, 0.f }; // InitialRotationRate 분포의 min/max 초당 각도
    Vec2 rotationRateScaleByLife{ 1.f, 1.f };    // RotationRateScaleByLife의 시작/종료 배율
    uint32 rotationOverLifeCurveKeyCount{ 2 };   // RotationOverLife compact curve key 수

#pragma region rotationOverLifeCurve* / rotationRateScaleByLifeCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키). rotationOverLifeCurveValues 단위 degree
    Vec4 rotationOverLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 rotationOverLifeCurveTimesBlock1{};
    Vec4 rotationOverLifeCurveValues{ 0.f, 0.f, 0.f, 0.f };
    Vec4 rotationOverLifeCurveValuesBlock1{};
    Vec4 rotationOverLifeCurveArriveTangents{};
    Vec4 rotationOverLifeCurveArriveTangentsBlock1{};
    Vec4 rotationOverLifeCurveLeaveTangents{};
    Vec4 rotationOverLifeCurveLeaveTangentsBlock1{};
    Vec4 rotationOverLifeCurveModes{ 1.f, 1.f, 0.f, 0.f };        // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 rotationOverLifeCurveModesBlock1{};
    uint32 rotationRateScaleByLifeCurveKeyCount{ 2 };             // RotationRateScaleByLife compact curve key 수
    Vec4 rotationRateScaleByLifeCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 rotationRateScaleByLifeCurveTimesBlock1{};
    Vec4 rotationRateScaleByLifeCurveValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 rotationRateScaleByLifeCurveValuesBlock1{};
    Vec4 rotationRateScaleByLifeCurveArriveTangents{};
    Vec4 rotationRateScaleByLifeCurveArriveTangentsBlock1{};
    Vec4 rotationRateScaleByLifeCurveLeaveTangents{};
    Vec4 rotationRateScaleByLifeCurveLeaveTangentsBlock1{};
    Vec4 rotationRateScaleByLifeCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 rotationRateScaleByLifeCurveModesBlock1{};
#pragma endregion
    PointParticleRandomSeedRuntimeDesc initialRotationSeed{};
    PointParticleRandomSeedRuntimeDesc initialRotationRateSeed{};
};

// Spawn 모듈의 burst 항목을 runtime emitter가 소비할 수 있게 압축한 payload
struct PointParticleBurstDesc
{
    float time{ 0.f };   // emitter 시작 후 burst가 실행될 시간
    uint32 count{ 16u }; // 해당 시간에 추가로 활성화할 particle 수
};

// Spawn 모듈이 만든 연속/버스트 생성 규칙
struct PointParticleSpawnDesc
{
    bool processSpawnRate{ true };                            // 연속 spawn rate를 적용할지 여부
    float spawnRate{ 10.f };                                  // 초당 생성할 particle 수
    Vec2 spawnRateRange{ 10.f, 10.f };                        // loop 시작 시 seed 기반으로 샘플할 spawn rate 범위
    PointParticleRandomSeedRuntimeDesc spawnRateSeed{};
    float spawnRateScale{ 1.f };                              // spawn rate에 곱할 배율
    Vec2 spawnRateScaleRange{ 1.f, 1.f };                     // loop 시작 시 seed 기반으로 샘플할 spawn rate scale 범위
    PointParticleRandomSeedRuntimeDesc spawnRateScaleSeed{};
    bool processBurstList{ true };                            // burst 목록을 적용할지 여부
    vector<PointParticleBurstDesc> bursts{};                  // emitter time 기준 1회성 burst 목록
    float burstScale{ 1.f };                                  // burst count에 곱할 배율
    Vec2 burstScaleRange{ 1.f, 1.f };                         // burst 실행 시점에 seed 기반으로 샘플할 burst scale 범위
    PointParticleRandomSeedRuntimeDesc burstScaleSeed{};
    uint32 maxActiveCount{ 64u };                             // 동시에 살아 있을 수 있는 particle 상한
    PointParticleFloatCurveRuntimeDesc spawnRateCurve{};      // SpawnRate ConstantCurve를 loop phase 기준으로 평가하는 payload
    PointParticleFloatCurveRuntimeDesc spawnRateScaleCurve{}; // SpawnRateScale ConstantCurve를 loop phase 기준으로 평가하는 payload
    PointParticleFloatCurveRuntimeDesc burstScaleCurve{};     // BurstScale ConstantCurve를 burst time phase 기준으로 평가하는 payload
};

// InitialLocation 모듈이 만든 생성 시점 위치 offset 분포
struct PointParticleInitialLocationDesc
{
    bool enabled{ false };                 // authoring InitialLocation 모듈이 preview/runtime에 연결되었는지 여부
    Vec3 minOffset{ -0.5f, -0.5f, -0.5f }; // emitter center 기준 최소 시작 offset
    Vec3 maxOffset{ 0.5f, 0.5f, 0.5f };    // emitter center 기준 최대 시작 offset
};

// SphereLocation 모듈이 구 내부/표면 중 어디에서 생성 위치를 샘플링할지 결결정
enum class PointParticleSphereLocationMode : uint32
{
    Volume = 0,  // 구 내부 부피에서 샘플링
    Surface = 1, // 구 표면에서 샘플링
};

// SphereLocation 모듈이 Surface 위치를 고르는 방식
enum class PointParticleSphereLocationPlacementMode : uint32
{
    Random = 0,
    EvenByParticleIndex = 1,
};

// SphereLocation 모듈이 만든 생성 시점 구형 위치 offset 분포
struct PointParticleSphereLocationDesc
{
    bool enabled{ false };                                  // authoring SphereLocation 모듈이 preview/runtime에 연결되었는지 여부
    Vec3 offset{ 0.f, 0.f, 0.f };                           // emitter center 기준 구 중심 offset
    float radius{ 1.f };                                    // 구 샘플링 반지름. 0 이하면 no-op으로 처리
    PointParticleSphereLocationMode mode{
        PointParticleSphereLocationMode::Volume };          // 구 내부/표면 샘플링 정책
    PointParticleSphereLocationPlacementMode placementMode{
        PointParticleSphereLocationPlacementMode::Random }; // Surface 모드에서 구 표면 방향을 고르는 정책
};

// PlaneRadialLocation 모듈이 위치를 샘플링할 emitter-local 평면
enum class PointParticlePlaneRadialLocationPlane : uint32
{
    XY = 0,
    XZ = 1,
    YZ = 2,
    CameraFacing = 3,
};

// PlaneRadialLocation 모듈이 평면 위에서 사용할 샘플링 형태
enum class PointParticlePlaneRadialLocationShape : uint32
{
    Rectangle = 0,
    Disc = 1,
    Ring = 2,
    Arc = 3,
};

// PlaneRadialLocation 모듈이 polar angle을 고르는 방식
enum class PointParticlePlaneRadialLocationPlacementMode : uint32
{
    Random = 0,
    EvenByParticleIndex = 1,
};

// PlaneRadialLocation 모듈이 만든 생성 시점 평면/방사 위치 offset 분포
struct PointParticlePlaneRadialLocationDesc
{
    bool enabled{ false };
    PointParticlePlaneRadialLocationPlane plane{
        PointParticlePlaneRadialLocationPlane::XY };             // 샘플링할 emitter-local 평면
    PointParticlePlaneRadialLocationShape shape{
        PointParticlePlaneRadialLocationShape::Disc };           // 평면 위 위치 샘플링 형태
    PointParticlePlaneRadialLocationPlacementMode placementMode{
        PointParticlePlaneRadialLocationPlacementMode::Random }; // polar angle 선택 정책
    Vec3 offset{ 0.f, 0.f, 0.f };                                // emitter center 기준 평면 샘플 중심 offset
    Vec2 uRange{ -0.5f, 0.5f };                                  // Rectangle 샘플링에서 plane U축 범위
    Vec2 vRange{ -0.5f, 0.5f };                                  // Rectangle 샘플링에서 plane V축 범위
    Vec2 radiusRange{ 0.f, 1.f };                                // Disc/Ring/Arc 샘플링 반지름 범위
    Vec2 angleDegreesRange{ 0.f, 360.f };                        // Disc/Ring/Arc 샘플링 각도 범위
    PointParticleRandomSeedRuntimeDesc uSeed{};                  // Rectangle U random sample seed salt
    PointParticleRandomSeedRuntimeDesc vSeed{};                  // Rectangle V random sample seed salt
    PointParticleRandomSeedRuntimeDesc radiusSeed{};             // Polar radius random sample seed salt
    PointParticleRandomSeedRuntimeDesc angleDegreesSeed{};       // Polar angle random sample seed salt
    float thickness{ 0.f };                                      // plane normal 방향 두께. 0이면 평면에 부착
};

// CylinderLocation 모듈이 원통 길이축으로 사용할 emitter-local axis
enum class PointParticleCylinderLocationAxis : uint32
{
    LocalX = 0,
    LocalY = 1,
    LocalLook = 2,
};

// CylinderLocation 모듈이 원통에서 위치를 샘플링할 영역
enum class PointParticleCylinderLocationMode : uint32
{
    SideSurface = 0,
    Volume = 1,
};

// CylinderLocation 모듈이 원주 angle을 고르는 방식
enum class PointParticleCylinderLocationPlacementMode : uint32
{
    Random = 0,
    EvenByParticleIndex = 1,
};

// CylinderLocation 모듈이 만든 생성 시점 원통 위치 offset 분포
struct PointParticleCylinderLocationDesc
{
    bool enabled{ false };
    PointParticleCylinderLocationAxis axis{
        PointParticleCylinderLocationAxis::LocalLook };       // 원통 길이축으로 사용할 emitter-local axis
    PointParticleCylinderLocationMode mode{
        PointParticleCylinderLocationMode::SideSurface };     // 원통 옆면/shell 또는 부피 샘플링 정책
    PointParticleCylinderLocationPlacementMode placementMode{
        PointParticleCylinderLocationPlacementMode::Random }; // 원주 angle 선택 정책
    Vec3 offset{ 0.f, 0.f, 0.f };                             // emitter center 기준 원통 중심 offset
    Vec2 radiusRange{ 0.f, 1.f };                             // 원통 반지름 또는 shell 두께 범위
    Vec2 heightRange{ -0.5f, 0.5f };                          // 원통 길이축 방향 위치 범위
    Vec2 angleDegreesRange{ 0.f, 360.f };                     // 원주 angle 범위
    PointParticleRandomSeedRuntimeDesc radiusSeed{};
    PointParticleRandomSeedRuntimeDesc heightSeed{};
    PointParticleRandomSeedRuntimeDesc angleDegreesSeed{};
};

// CylinderOrientation 모듈이 cylinder sample frame에서 고를 방향
enum class PointParticleCylinderOrientationMode : uint32
{
    None = 0,
    FaceRadialOut = 1,
    FaceRadialIn = 2,
    FaceTangentCW = 3,
    FaceTangentCCW = 4,
    FaceCylinderAxisPositive = 5,
    FaceCylinderAxisNegative = 6,
};

// PlaneRadialOrientation 모듈이 회전 결과를 적용할 runtime 대상
enum class PointParticlePlaneRadialOrientationTargetKind : uint32
{
    Auto = 0,
    Sprite2D = 1,
    Mesh3D = 2,
};

// PlaneRadialOrientation 모듈이 radial sample frame에서 고를 방향
enum class PointParticlePlaneRadialOrientationMode : uint32
{
    None = 0,
    FaceRadialOut = 1,
    FaceRadialIn = 2,
    FaceTangentCW = 3,
    FaceTangentCCW = 4,
    FacePlaneNormal = 5,
};

// PlaneRadialOrientation 모듈이 mesh local 축을 지정할 때 사용하는 축 token
enum class PointParticlePlaneRadialOrientationAxis : uint32
{
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};

// PlaneRadialOrientation 모듈이 만든 radial frame 기반 초기 회전 payload
struct PointParticlePlaneRadialOrientationDesc
{
    bool enabled{ false };
    PointParticlePlaneRadialOrientationTargetKind targetKind{
        PointParticlePlaneRadialOrientationTargetKind::Auto }; // 회전 적용 대상 자동/스프라이트/메시 정책
    PointParticlePlaneRadialOrientationMode orientationMode{
        PointParticlePlaneRadialOrientationMode::None };       // radial frame에서 고를 초기 방향
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ };  // mesh forward로 볼 local 축
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY };  // mesh up으로 볼 local 축
    float tiltDegrees{ 0.f };                                  // radial 방향에서 추가로 기울일 각도
    float rollOffsetDegrees{ 0.f };                            // 최종 방향 기준 roll 보정 각도
};

// SphereRadialOrientation 모듈이 sphere sample frame에서 고를 방향
enum class PointParticleSphereRadialOrientationMode : uint32
{
    None = 0,
    FaceRadialOut = 1,
    FaceRadialIn = 2,
};

// SphereRadialOrientation 모듈이 만든 구형 radial frame 기반 초기 mesh 회전 payload
struct PointParticleSphereRadialOrientationDesc
{
    bool enabled{ false };
    PointParticleSphereRadialOrientationMode orientationMode{
        PointParticleSphereRadialOrientationMode::None };     // sphere radial frame에서 고를 초기 방향
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ }; // mesh forward로 볼 local 축
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY }; // mesh up으로 볼 local 축
    float tiltDegrees{ 0.f };                                 // radial 방향에서 추가로 기울일 각도
    float rollOffsetDegrees{ 0.f };                           // 최종 방향 기준 roll 보정 각도
};

// CylinderOrientation 모듈이 만든 cylinder sample frame 기반 초기 회전 payload
struct PointParticleCylinderOrientationDesc
{
    bool enabled{ false };
    PointParticlePlaneRadialOrientationTargetKind targetKind{
        PointParticlePlaneRadialOrientationTargetKind::Auto }; // 회전 적용 대상 자동/스프라이트/메시 정책
    PointParticleCylinderOrientationMode orientationMode{
        PointParticleCylinderOrientationMode::None };          // cylinder frame에서 고를 초기 방향
    bool followOrbitOverLife{ false };                         // OrbitOverLife 위치 회전을 sprite 방향에도 반영할지 여부
    PointParticlePlaneRadialOrientationAxis meshForwardAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveZ };  // mesh forward로 볼 local 축
    PointParticlePlaneRadialOrientationAxis meshUpAxis{
        PointParticlePlaneRadialOrientationAxis::PositiveY };  // mesh up으로 볼 local 축
    float tiltDegrees{ 0.f };                                  // cylinder orientation 결과에 더하는 pitch/tilt 보정
    float rollOffsetDegrees{ 0.f };                            // 최종 방향 기준 roll 보정 각도
};

// OrbitOverLife 모듈이 emitter-local offset을 회전시킬 plane
enum class EffectOrbitPlane : uint32
{
    XY = 0,
    XZ = 1,
    YZ = 2,
};

// OrbitOverLife 모듈을 sprite/mesh runtime이 공유해 소비하는 payload
struct EffectOrbitOverLifeRuntimeDesc
{
    bool enabled{ false };                          // OrbitOverLife 모듈이 preview/runtime에 연결되었는지 여부
    EffectOrbitPlane plane{ EffectOrbitPlane::XY }; // emitter-local offset을 회전시킬 평면
    Vec2 angleDegreesOverLife{ 0.f, 0.f };          // 수명 시작/종료 시 누적 orbit 각도 fallback
    uint32 angleCurveKeyCount{ 2 };                 // orbit angle compact curve key 수

#pragma region angleCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키). Values 단위 degree
    Vec4 angleCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 angleCurveTimesBlock1{};
    Vec4 angleCurveValues{ 0.f, 0.f, 0.f, 0.f };
    Vec4 angleCurveValuesBlock1{};
    Vec4 angleCurveArriveTangents{};
    Vec4 angleCurveArriveTangentsBlock1{};
    Vec4 angleCurveLeaveTangents{};
    Vec4 angleCurveLeaveTangentsBlock1{};
    Vec4 angleCurveModes{ 1.f, 1.f, 0.f, 0.f }; // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 angleCurveModesBlock1{};
#pragma endregion
    Vec2 radiusScaleOverLife{ 1.f, 1.f };       // 수명 시작/종료 시 orbit radius 배율 fallback
    uint32 radiusScaleCurveKeyCount{ 2 };       // radius scale compact curve key 수

#pragma region radiusScaleCurve*
    // 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
    Vec4 radiusScaleCurveTimes{ 0.f, 1.f, 0.f, 0.f };
    Vec4 radiusScaleCurveTimesBlock1{};
    Vec4 radiusScaleCurveValues{ 1.f, 1.f, 0.f, 0.f };
    Vec4 radiusScaleCurveValuesBlock1{};
    Vec4 radiusScaleCurveArriveTangents{};
    Vec4 radiusScaleCurveArriveTangentsBlock1{};
    Vec4 radiusScaleCurveLeaveTangents{};
    Vec4 radiusScaleCurveLeaveTangentsBlock1{};
    Vec4 radiusScaleCurveModes{ 1.f, 1.f, 0.f, 0.f };  // 0 Constant, 1 Linear, 2 CurveAutoClamped.
    Vec4 radiusScaleCurveModesBlock1{};
#pragma endregion
};

// 인스턴스 생성에 공통으로 필요한 spawn 규칙
struct InstanceSpawnDesc
{
    uint32 instanceCount{}; // 생성할 인스턴스 수
    Vec3 center{};          // 스폰 영역 중심
    Vec3 range{};           // 스폰 영역 크기
    Vec2 scale{};           // 랜덤 스케일 범위
};

// compute shader가 instance payload를 갱신하는 compute-first sprite particle desc
struct ComputePointParticleDesc : public InstanceSpawnDesc
{
    Vec2 sizeMin{ 0.12f, 0.12f };                                     // particle 기본 가로/세로 크기 하한
    Vec2 sizeMax{ 0.28f, 0.28f };                                     // particle 기본 가로/세로 크기 상한
    PointParticleRandomSeedRuntimeDesc initialSizeSeed{};
    Vec2 lifeTime{};                                                  // compute shader가 누적 시간을 계산할 때 사용할 생존 시간 범위
    PointParticleFloatCurveRuntimeDesc lifetimeCurve{};               // Lifetime ConstantCurve를 spawn event phase 기준으로 평가하는 payload
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f };                         // compute sprite preview 시작 색상/알파 하한
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f };                         // compute sprite preview 시작 색상/알파 상한
    PointParticleRandomSeedRuntimeDesc initialColorSeed{};            // InitialColor RGB random sample seed salt
    PointParticleRandomSeedRuntimeDesc initialAlphaSeed{};            // InitialColor Alpha random sample seed salt
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };                           // compute sprite preview 종료 색상/알파 하한
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };                           // compute sprite preview 종료 색상/알파 상한
    PointParticleRandomSeedRuntimeDesc colorOverLifeSeed{};           // ColorOverLife RGB random sample seed salt
    PointParticleRandomSeedRuntimeDesc alphaOverLifeSeed{};           // ColorOverLife Alpha random sample seed salt
    PointParticleColorOverLifeCurveDesc colorOverLifeCurve{};         // ConstantCurve ColorOverLife payload
    Vec4 coreColorRgbParticleLifeUniformParams{};                     // x: enabled.
    Vec4 coreColorRgbParticleLifeUniformMin{ 1.f, 0.85f, 0.45f, 0.f };
    Vec4 coreColorRgbParticleLifeUniformMax{ 1.f, 0.85f, 0.45f, 0.f };
    XMUINT4 coreColorRgbParticleLifeUniformSeed{};                    // x: seed salt after playback-seed resolution.
    Vec2 radius{};                                                    // 화면 확인용 원형 이동 반경 범위. authoring 의미로 승격하지 않음
    uint32 subUVRows{ 1 };                                            // atlas row 수
    uint32 subUVCols{ 1 };                                            // atlas column 수
    PointParticleSubUVFrameOverLifeDesc subUvFrameOverLife{};         // 수명 진행률 기준 frame 선택 규칙
    PointParticleSizeByLifeDesc sizeByLife{};                         // 수명 진행률 기준 크기 배율 규칙
    PointParticleMotionDesc motion{};                                 // motion 모듈들이 만든 compute update 규칙
    float playbackDuration{ 1.f };                                    // emitter time 기준 curve 평가에 사용할 loop duration
    EffectOrbitOverLifeRuntimeDesc orbitOverLife{};                   // OrbitOverLife 모듈이 만든 emitter-local pivot 회전 payload
    PointParticleRotationDesc rotation{};                             // rotation 모듈들이 만든 sprite 회전 규칙
    PointParticleSpriteTiltDesc spriteTilt{};                         // SpriteTilt 모듈들이 만든 카드 면 기울기 규칙
    PointParticleSpawnDesc spawn{};                                   // Spawn 모듈이 만든 활성화/버스트 생성 규칙
    PointParticleRandomSeedRuntimeDesc lifetimeSeed{};
    PointParticleInitialLocationDesc initialLocation{};               // InitialLocation 모듈이 만든 시작 위치 offset 분포
    PointParticleSphereLocationDesc sphereLocation{};                 // SphereLocation 모듈이 만든 시작 위치 구형 offset 분포
    PointParticlePlaneRadialLocationDesc planeRadialLocation{};       // PlaneRadialLocation 모듈이 만든 시작 위치 평면/방사 offset 분포
    PointParticleCylinderLocationDesc cylinderLocation{};             // CylinderLocation 모듈이 만든 시작 위치 원통 offset 분포
    PointParticlePlaneRadialOrientationDesc planeRadialOrientation{}; // PlaneRadialOrientation 모듈이 만든 sprite 2D 초기 회전 payload
    PointParticleCylinderOrientationDesc cylinderOrientation{};       // CylinderOrientation 모듈이 만든 cylinder frame 기반 sprite 방향 payload
    PointParticleRandomSeedRuntimeDesc initialLocationSeed{};
    PointParticleRandomSeedRuntimeDesc sphereLocationSeed{};
    PointParticleRandomSeedRuntimeDesc subUVRandomFrameSeed{};
    Vec3 emitterRight{ 1.f, 0.f, 0.f };                               // emitter local X축을 world 방향으로 변환할 basis
    Vec3 emitterUp{ 0.f, 1.f, 0.f };                                  // emitter local Y축을 world 방향으로 변환할 basis
    Vec3 emitterLook{ 0.f, 0.f, 1.f };                                // emitter local Z축을 world 방향으로 변환할 basis
    uint32 effectPlaybackSeed{ 0 };                                   // effect root 재생 1회 단위 random seed
    bool useMaxDrawCount{ false };                                    // true면 draw 제출 수만 별도로 제한
    uint32 maxDrawCount{ 0 };                                         // 0이면 instanceCount 기준으로 제한
    PointParticleDrawMode drawMode{                                   // compute particle 경로의 정식 draw 제출 방식
        PointParticleDrawMode::DrawIndexedInstancedIndirect };
    uint32 computeShaderLevelIndex{};                                 // compute shader prototype 등록 level index
    wstring computeShaderPrototypeTag{};                              // instance payload를 갱신할 compute shader component prototype tag
};

NS_END
