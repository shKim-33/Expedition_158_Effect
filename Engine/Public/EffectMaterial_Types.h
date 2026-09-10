#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

enum class EffectTextureUVTilingMode : uint32
{
    Wrap = 0,    // distance UV에 tiling을 곱하고 wrap sampler로 반복
    Stretch = 1, // visible length 기준 0..1로 정규화하고 clamp sampler로 전개
    Clamp = 2,   // distance UV에 tiling을 곱하되 clamp sampler로 범위 밖을 고결정
    Raw = 3,     // shader가 받은 distance UV를 tiling/stretch 없이 그대로 사용
    Mirror = 4,  // distance UV에 tiling을 곱하고 shader-side mirror repeat로 반복
};

// effect material texture slot의 U/V축별 UV 해석 정책
struct EffectMaterialUVAxisPolicy
{
    EffectTextureUVTilingMode uPolicy{ EffectTextureUVTilingMode::Stretch };
    EffectTextureUVTilingMode vPolicy{ EffectTextureUVTilingMode::Stretch };
};

enum class EffectMaterialUVRotation : uint32
{
    None = 0,
    Rotate90CW,
    Rotate180,
    Rotate90CCW,
};

enum class EffectMaterialFamily : uint32
{
    Default = 0,      // 기존 color/coverage 중심 effect material family
    SpriteDistortion, // scene color snapshot을 screen-space offset으로 다시 샘플링하는 sprite distortion family
    MeshGlass,        // mesh 표면에 투명도, rim, fake light 반사를 입히는 mesh 전용 glass family
    END,
};

// SpriteDistortion family에서 screen UV offset 강도에 곱할 renderer-local 영향 범위를 선택
enum class EffectDistortionShapeMode : uint32
{
    None = 0,      // 기존 square card 전체 distortion 동작
    Radial,        // sprite UV 중심 기준 soft circle/aura 계열 영향 범위
    Ring,          // sprite UV 중심 기준 shockwave rim 계열 영향 범위
    ShockwaveRing, // 수명 진행률로 확장하며 screen pixel 기준 두께를 유지하는 shockwave ring
    AirSheath = 4, // Trail 폭 방향의 분석적 lens profile로 공기막 굴절을 생성
    END,
};

// AirSheath가 기존 Flow Texture를 해석하는 방식. 비-AirSheath renderer는 이 값을 소비하지 않음
enum class EffectAirSheathMapInterpretation : uint32
{
    VectorField = 0,
    HeightGradient,
    ScalarModulation,
    END,
};

enum class EffectAirSheathMapScalarSource : uint32
{
    Red = 0,
    Green,
    Blue,
    Alpha,
    Luminance,
    END,
};

enum class EffectAirSheathMapVectorSpace : uint32
{
    Screen = 0,
    TrailLocal,
    BaseField,
    END,
};

enum class EffectAirSheathMapComposition : uint32
{
    Replace = 0,
    Add,
    Modulate,
    END,
};

enum class EffectMaterialBlendMode : uint32
{
    AlphaBlend = 0,
    Additive,
    Modulate,
    Masked,
    END,
};

enum class EffectMaterialAdditiveColorSource : uint32
{
    MainRGB = 0,
    TintedRGB,
    EmissiveColor,
    ConstantColor,
    END,
};

enum class EffectMaterialAdditiveAmountSource : uint32
{
    Alpha = 0,
    Red,
    Luminance,
    Mask,
    One,
    END,
};

enum class EffectMaterialAdditiveCoveragePolicy : uint32
{
    AmountOnly = 0,
    CoverageAndAmount,
    Independent,
    END,
};

struct EffectMaterialAdditiveContributionData
{
    EffectMaterialAdditiveColorSource colorSource{ EffectMaterialAdditiveColorSource::MainRGB };
    EffectMaterialAdditiveAmountSource amountSource{ EffectMaterialAdditiveAmountSource::Alpha };
    EffectMaterialAdditiveCoveragePolicy coveragePolicy{ EffectMaterialAdditiveCoveragePolicy::AmountOnly };
    float intensityScale{ 1.f };
    bool blackNeutral{ true };
    Color emissiveColor{ 1.f, 1.f, 1.f, 1.f };
    Color constantColor{ 1.f, 1.f, 1.f, 1.f };
};

struct EffectMaterialCoreEmissiveData
{
    bool enabled{ false };
    Color coreColor{ 1.f, 0.85f, 0.45f, 1.f };
    float corePower{ 4.f };
    float coreIntensity{ 2.f };
    float outerPower{ 1.f };
    float outerIntensity{ 1.f };
};

// effect material preset/source에서 복사되어 runtime/authoring 쪽이 함께 보는 최소 material instance 값
// sprite effect 경로에서는 mainTexturePath를 lit base color가 아니라 emissive color + opacity source로 해석
struct EffectMaterialInstanceData
{
    string sourcePresetName{};                                            // UI 표시용 원본 preset 이름. 저장 identity나 live reference로 쓰지 않음
    EffectMaterialFamily materialFamily{ EffectMaterialFamily::Default }; // material shader/pass family를 고르는 상위 분류
    string mainTextureGuid{};                                             // 주 텍스처 asset identity. resolve 실패 시 path를 fallback으로 사용
    string mainTexturePath{};                                             // 주 텍스처 resource-relative fallback/debug hint
    string noiseTextureGuid{};                                            // 선택 noise 텍스처 asset identity
    string noiseTexturePath{};                                            // 선택 noise 텍스처 resource-relative fallback/debug hint
    string maskTextureGuid{};                                             // 선택 mask 텍스처 asset identity
    string maskTexturePath{};                                             // 선택 mask 텍스처 resource-relative fallback/debug hint
    string flowTextureGuid{};                                             // SpriteDistortion screen UV offset source flow texture identity.
    string flowTexturePath{};                                             // SpriteDistortion flow texture의 resource-relative fallback

    Color tint{ 1.f, 1.f, 1.f, 1.f }; // main texture RGB emissive-like 색상에 곱할 색상 계수

    float intensity{ 1.f };       // sprite 밝기/발광감 계수
    float opacityPower{ 1.f };    // opacity source(alpha/red/luminance)의 sharpening 계수
    float alphaMultiplier{ 1.f }; // 최종 effect alpha에 곱할 투명도 배율
    float noiseStrength{ 0.f };   // noise modulation 강도
    float alphaCutoff{ 0.f };     // alpha cutoff 후보 값. 소비하지 않는 renderer에서는 보존만 함
    float alphaErosion{ 0.f };    // mask/noise 기반 alpha erosion 후보 값. 소비하지 않는 renderer에서는 보존만 함
    string noiseSource{ "Red" };  // noise texture에서 scalar를 뽑을 채널/방식
    string maskSource{ "Alpha" }; // mask texture에서 coverage를 뽑을 채널/방식
    bool noiseInvert{ false };    // noise scalar를 1-x로 뒤집어 사용
    bool maskInvert{ false };     // mask coverage를 1-x로 뒤집어 사용

    Vec2 mainUVScale{ 1.f, 1.f };                                                      // main texture UV scale 후보 값
    Vec2 mainUVOffset{ 0.f, 0.f };                                                     // main texture UV 시작 위치를 정적으로 이동
    Vec2 mainUVScrollSpeed{ 0.f, 0.f };                                                // main texture UV scroll speed 후보 값
    EffectTextureUVTilingMode mainUVTilingMode{ EffectTextureUVTilingMode::Stretch };  // main texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy mainUVPolicy{};                                         // main texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation mainUVRotation{ EffectMaterialUVRotation::None };         // main texture UV를 90도 단위로 회전하는 방식
    Vec2 noiseUVScale{ 1.f, 1.f };                                                     // noise texture UV scale 후보 값
    Vec2 noiseUVOffset{ 0.f, 0.f };                                                    // noise texture UV 시작 위치를 정적으로 이동
    Vec2 noiseUVScrollSpeed{ 0.f, 0.f };                                               // noise texture UV scroll speed 후보 값
    EffectTextureUVTilingMode noiseUVTilingMode{ EffectTextureUVTilingMode::Stretch }; // noise texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy noiseUVPolicy{};                                        // noise texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation noiseUVRotation{ EffectMaterialUVRotation::None };        // noise texture UV를 90도 단위로 회전하는 방식
    Vec2 maskUVScale{ 1.f, 1.f };                                                      // mask texture UV scale 후보 값
    Vec2 maskUVOffset{ 0.f, 0.f };                                                     // mask texture UV 시작 위치를 정적으로 이동
    Vec2 maskUVScrollSpeed{ 0.f, 0.f };                                                // mask texture UV scroll speed 후보 값
    EffectTextureUVTilingMode maskUVTilingMode{ EffectTextureUVTilingMode::Stretch };  // mask texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy maskUVPolicy{};                                         // mask texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation maskUVRotation{ EffectMaterialUVRotation::None };         // mask texture UV를 90도 단위로 회전하는 방식
    Vec2 flowUVScale{ 1.f, 1.f };                                                      // flow texture UV scale 후보 값
    Vec2 flowUVOffset{ 0.f, 0.f };                                                     // flow texture UV 시작 위치를 정적으로 이동
    Vec2 flowUVScrollSpeed{ 0.f, 0.f };                                                // flow texture UV scroll speed 후보 값
    EffectTextureUVTilingMode flowUVTilingMode{ EffectTextureUVTilingMode::Stretch };  // flow texture UV tiling 해석 방식
    EffectMaterialUVAxisPolicy flowUVPolicy{};                                         // flow texture U/V축별 UV 해석 방식
    EffectMaterialUVRotation flowUVRotation{ EffectMaterialUVRotation::None };         // flow texture UV를 90도 단위로 회전하는 방식

    EffectMaterialBlendMode blendMode{ EffectMaterialBlendMode::AlphaBlend };         // effect material blend/compose mode
    string opacitySource{ "Alpha" };                                                  // main texture에서 opacity를 뽑을 채널/방식
    EffectMaterialAdditiveContributionData additive{};                                // Additive blend에서 더할 contribution 해석 규칙
    EffectMaterialCoreEmissiveData coreEmissive{};                                    // alpha-derived inner core/outer halo shaping 값
    float refractionIntensity{ 0.f };                                                 // SpriteDistortion screen UV offset 강도
    float refractionPresence{ 1.f };                                                  // SpriteDistortion refraction 영향도 조정용 authored scalar
    EffectDistortionShapeMode distortionShapeMode{ EffectDistortionShapeMode::None }; // SpriteDistortion 영향 범위 weight mode
    EffectAirSheathMapInterpretation airSheathMapInterpretation{ EffectAirSheathMapInterpretation::VectorField };
    EffectAirSheathMapScalarSource airSheathMapXSource{ EffectAirSheathMapScalarSource::Red };
    EffectAirSheathMapScalarSource airSheathMapYSource{ EffectAirSheathMapScalarSource::Green };
    EffectAirSheathMapVectorSpace airSheathMapVectorSpace{ EffectAirSheathMapVectorSpace::Screen };
    EffectAirSheathMapComposition airSheathMapComposition{ EffectAirSheathMapComposition::Replace };
    float airSheathMapInfluence{ 0.f };                                               // 0이면 AirSheath base wake만 사용
    float distortionShapeRadius{ 0.45f };                                             // sprite UV 중심에서 shape 기준 반경
    float distortionShapeThickness{ 0.10f };                                          // Ring은 UV 폭, ShockwaveRing은 screen pixel half-width
    float distortionShapeSoftness{ 0.15f };                                           // Ring은 UV falloff, ShockwaveRing은 screen pixel falloff
    float glassAlpha{ 0.18f };                                                        // MeshGlass 기본 표면 투명도
    float glassAlphaPower{ 1.f };                                                     // MeshGlass texture coverage sharpen/soften 보정
    float glassNormalStrength{ 1.f };                                                 // MeshGlass model normal 반응 강도
    Color glassRimColor{ 1.f, 1.f, 1.f, 1.f };                                        // MeshGlass view-angle rim 색상
    float glassRimIntensity{ 1.5f };                                                  // MeshGlass rim 밝기 배율
    float glassRimPower{ 3.f };                                                       // MeshGlass rim falloff 곡선의 날카로움
    Vec3 glassLightDirection{ -0.35f, 0.65f, -0.65f };                                // MeshGlass world-space fake light 방향
    Color glassLightColor{ 1.f, 1.f, 1.f, 1.f };                                      // MeshGlass fake light 색상
    float glassLightIntensity{ 1.2f };                                                // MeshGlass fake light 밝기 배율
    float glassSpecularPower{ 48.f };                                                 // MeshGlass highlight 날카로움
    float glassSpecularSoftness{ 1.f };                                               // MeshGlass highlight 폭 보정
    float glassMainInfluence{ 0.25f };                                                // MeshGlass main texture tint/pattern 반영 강도
    float glassNoiseBreakup{ 0.35f };                                                 // MeshGlass noise texture breakup 강도
    float glassMaskStrength{ 1.f };                                                   // MeshGlass mask coverage 강도
    bool twoSided{ false };                                                           // render state 후보 값. 소비하지 않는 renderer에서는 보존만 함
    uint32 subUVRows{ 1 };                                                            // atlas row 수
    uint32 subUVCols{ 1 };                                                            // atlas column 수
};

NS_END
