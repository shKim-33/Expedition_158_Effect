#pragma once

// 이미터 cpp마다 같은 본문으로 복제해 쓰던 순수 연산 helper를 모은다.

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"
#include "TransformCom.h"

namespace Client::EmitterMath
{
//## SubUV
// SubUV 시작/끝 프레임을 clamp한 재생 구간이다.
struct SubUVFrameRange
{
    uint32 startFrame{};
    uint32 frameCount{ 1u };
    uint32 rangeCount{ 1u };
};

// 시작/끝 프레임을 프레임 수 안으로 clamp하고, 끝이 시작보다 앞이면 한 바퀴 돈 구간으로 해석한다.
inline SubUVFrameRange Resolve_SubUVFrameRange(uint32 startFrame, uint32 endFrame, uint32 frameCount)
{
    const uint32 safeFrameCount = max(1u, frameCount);
    const uint32 lastFrame = safeFrameCount - 1u;
    const uint32 clampedStart = min(startFrame, lastFrame);
    const uint32 clampedEnd = min(endFrame, lastFrame);
    const uint32 rangeCount =
        clampedEnd >= clampedStart
        ? clampedEnd - clampedStart + 1u
        : safeFrameCount - clampedStart + clampedEnd + 1u;
    return SubUVFrameRange{ clampedStart, safeFrameCount, max(1u, rangeCount) };
}

// 구간 안의 offset 번째 프레임을 전체 프레임 인덱스로 돌려준다.
inline uint32 Resolve_SubUVFrameInRange(const SubUVFrameRange& range, uint32 frameOffset)
{
    return (range.startFrame + min(frameOffset, range.rangeCount - 1u)) % range.frameCount;
}

// 프레임 offset과 위상 offset을 합쳐 구간 안 재생 위치를 구한다. loop면 감기고, 아니면 끝 프레임에 머문다.
inline uint32 Resolve_SubUVPlaybackOffset(const SubUVFrameRange& range, uint32 frameOffset, uint32 phaseOffset, bool loop)
{
    const uint32 offset = frameOffset + phaseOffset;
    return loop ? offset % range.rangeCount : min(offset, range.rangeCount - 1u);
}

//## Interpolation
// Catmull-Rom 스플라인에서 p1~p2 구간의 ratio 위치를 구한다.
inline Vec3 Catmull_Rom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float ratio)
{
    const float t2 = ratio * ratio;
    const float t3 = t2 * ratio;

    return (p1 * 2.f
            + (p2 - p0) * ratio
            + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2
            + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3)
           * 0.5f;
}

// 최소/최대 색의 중간 색을 구한다.
inline Vec4 Average_Color(const Vec4& minColor, const Vec4& maxColor)
{
    return Vec4{
        (minColor.x + maxColor.x) * 0.5f,
        (minColor.y + maxColor.y) * 0.5f,
        (minColor.z + maxColor.z) * 0.5f,
        (minColor.w + maxColor.w) * 0.5f
    };
}

//## Curve
// Vec4의 index번째 성분을 읽는다. 범위를 벗어나면 x를 돌려준다.
inline float Read_Vec4Component(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 0:
        return values.x;
    case 1:
        return values.y;
    case 2:
        return values.z;
    case 3:
        return values.w;
    default:
        return values.x;
    }
}

// Vec4 두 개를 8칸 배열로 이어 보고 index번째 성분을 읽는다.
inline float Read_Vec4Component(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
{
    if (index < 4u)
        return Read_Vec4Component(valuesBlock0, index);

    return Read_Vec4Component(valuesBlock1, index - 4u);
}

// 압축 저장된 float 곡선을 phase(0~1)에서 평가한다. 곡선이 꺼져 있거나 구간 밖이면 fallbackValue를 쓴다.
inline float Evaluate_FloatCurve(const PointParticleFloatCurveRuntimeDesc& curve, float phase, float fallbackValue)
{
    if (!curve.enabled)
        return fallbackValue;

    const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, curve.curveKeyCount));
    const float clampedPhase = clamp(phase, 0.f, 1.f);
    if (keyCount == 1u || clampedPhase <= Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, 0u))
        return Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, 0u);

    for (uint32 index = 1u; index < keyCount; ++index)
    {
        const float rightTime = Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, index);
        if (clampedPhase > rightTime)
            continue;

        const float leftTime = Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, index - 1u);
        const float leftValue = Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, index - 1u);
        const float rightValue = Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, index);
        const float mode = Read_Vec4Component(curve.curveKeyModes, curve.curveKeyModesBlock1, index - 1u);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((clampedPhase - leftTime) / width, 0.f, 1.f);

        if (mode < 0.5f)
            return leftValue;

        if (mode >= 1.5f)
        {
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            const float leftLeave = Read_Vec4Component(curve.curveKeyLeaveTangents, curve.curveKeyLeaveTangentsBlock1, index - 1u) * width;
            const float rightArrive = Read_Vec4Component(curve.curveKeyArriveTangents, curve.curveKeyArriveTangentsBlock1, index) * width;
            return
                (2.f * t3 - 3.f * t2 + 1.f) * leftValue +
                (t3 - 2.f * t2 + ratio) * leftLeave +
                (-2.f * t3 + 3.f * t2) * rightValue +
                (t3 - t2) * rightArrive;
        }

        return lerp(leftValue, rightValue, ratio);
    }

    return clampedPhase > Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, keyCount - 1u)
           ? Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, keyCount - 1u)
           : fallbackValue;
}

//## Scale
// Transform 월드 행렬의 스케일을 0.0001 이상 양수로 돌려준다. Transform이 없거나 분해에 실패하면 1이다.
inline Vec3 Resolve_WorldScale(const Shared<TransformCom>& transform)
{
    if (transform == nullptr)
        return Vec3::One;

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;
    Matrix worldMatrix = transform->Get_WorldMatrix();
    if (!worldMatrix.Decompose(scale, rotation, translation))
        return Vec3::One;

    return Vec3{
        max(0.0001f, fabsf(scale.x)),
        max(0.0001f, fabsf(scale.y)),
        max(0.0001f, fabsf(scale.z))
    };
}

// 벡터를 성분별로 스케일과 곱한다.
inline Vec3 Scale_Vector3(const Vec3& value, const Vec3& scale)
{
    return Vec3{ value.x * scale.x, value.y * scale.y, value.z * scale.z };
}

inline Vec2 Scale_Vector2(const Vec2& value, const Vec2& scale)
{
    return Vec2{ value.x * scale.x, value.y * scale.y };
}

// 평면 종류에 맞는 두 축의 스케일을 고른다.
inline Vec2 Resolve_PlaneScale(PointParticlePlaneRadialLocationPlane plane, const Vec3& scale)
{
    switch (plane)
    {
    case PointParticlePlaneRadialLocationPlane::XZ:
        return Vec2{ scale.x, scale.z };
    case PointParticlePlaneRadialLocationPlane::YZ:
        return Vec2{ scale.y, scale.z };
    case PointParticlePlaneRadialLocationPlane::CameraFacing:
    case PointParticlePlaneRadialLocationPlane::XY:
    default:
        return Vec2{ scale.x, scale.y };
    }
}

// 원기둥 축에 맞춰 (반지름 스케일, 높이 스케일)을 고른다.
inline Vec2 Resolve_CylinderScale(PointParticleCylinderLocationAxis axis, const Vec3& scale)
{
    switch (axis)
    {
    case PointParticleCylinderLocationAxis::LocalX:
        return Vec2{ max(scale.y, scale.z), scale.x };
    case PointParticleCylinderLocationAxis::LocalY:
        return Vec2{ max(scale.x, scale.z), scale.y };
    case PointParticleCylinderLocationAxis::LocalLook:
    default:
        return Vec2{ max(scale.x, scale.y), scale.z };
    }
}
}
