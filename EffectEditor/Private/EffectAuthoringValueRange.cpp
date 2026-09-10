#include "EffectAuthoringValueRange.h"

NS_BEGIN(EffectEditor)

namespace
{
    struct AuthoringValueRangeEntry
    {
        AuthoringModuleType moduleType{};
        string_view fieldId{};
        AuthoringValueRange range{};
    };

    constexpr AuthoringValueRange MinOnly(float minValue)
    {
        return AuthoringValueRange{ true, false, minValue, 0.f };
    }

    constexpr AuthoringValueRange MinMax(float minValue, float maxValue)
    {
        return AuthoringValueRange{ true, true, minValue, maxValue };
    }

    constexpr AuthoringValueRangeEntry kAuthoringValueRanges[] = {
        { AuthoringModuleType::Required, "duration", MinOnly(0.0001f) },                         // 재생 길이는 양수 최소값으로 보정된다.
        { AuthoringModuleType::Required, "delay", MinOnly(0.f) },                                // 재생 지연은 음수가 되지 않도록 보정된다.
        { AuthoringModuleType::Spawn, "spawnRate", MinOnly(0.f) },                               // 스폰 속도 분포는 음수가 아닌 범위만 사용한다.
        { AuthoringModuleType::Spawn, "spawnRateScale", MinOnly(0.f) },                          // 스폰 속도 배율 분포는 음수가 아닌 범위만 사용한다.
        { AuthoringModuleType::Spawn, "burstScale", MinOnly(0.f) },                              // 버스트 배율 분포는 음수가 아닌 범위만 사용한다.
        { AuthoringModuleType::Lifetime, "lifeTime", MinOnly(0.0001f) },                         // over-life 계산을 위해 파티클 수명은 양수여야 한다.
        { AuthoringModuleType::SphereLocation, "radius", MinOnly(0.f) },                         // 구 반지름은 음수가 되지 않도록 보정된다.
        { AuthoringModuleType::PlaneRadialLocation, "radiusDistribution", MinOnly(0.f) },        // 반경 분포는 음수가 아닌 범위만 사용한다.
        { AuthoringModuleType::CylinderLocation, "radiusDistribution", MinOnly(0.f) },           // 원통 반경 분포는 음수가 아닌 범위만 사용한다.
        { AuthoringModuleType::Drag, "drag", MinOnly(0.f) },                                     // 드래그 분포는 음수가 아닌 범위만 사용한다.
        { AuthoringModuleType::InitialColor, "alpha", MinMax(0.f, 1.f) },                        // 작성된 알파 분포는 0..1 범위로 보정된다.
        { AuthoringModuleType::ColorOverLife, "alphaOverLife", MinMax(0.f, 1.f) },               // over-life 알파 끝값은 0..1 범위로 보정된다.
        { AuthoringModuleType::SubUVFrameOverLife, "frameIndex", MinOnly(0.f) },                 // 프레임 인덱스는 음수 의미가 없고 최대값은 이후 단계에서 정해진다.
        { AuthoringModuleType::SourceHistorySpriteTrailPathReplay, "drainCurve", MinOnly(0.f) }, // PathReplay drain curve는 정규화된 진행도 곡선이다.
    };
}

const AuthoringValueRange* Find_AuthoringValueRange(AuthoringModuleType moduleType, std::string_view fieldId)
{
    for (const AuthoringValueRangeEntry& entry : kAuthoringValueRanges)
    {
        if (entry.moduleType == moduleType && entry.fieldId == fieldId)
            return &entry.range;
    }

    return nullptr;
}

NS_END
