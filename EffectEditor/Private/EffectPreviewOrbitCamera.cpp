#include "EffectPreviewOrbitCamera.h"

NS_BEGIN(EffectEditor)

void EffectPreviewOrbitCamera::Reset(
    EffectPreviewOrbitCameraState& state,
    const Vec3& pivot,
    float distance,
    float yawDegrees,
    float pitchDegrees)
{
    state.pivot = pivot;
    state.distance = max(distance, 0.1f);
    state.yawDegrees = yawDegrees;
    state.pitchDegrees = pitchDegrees;
}

void EffectPreviewOrbitCamera::Orbit(
    EffectPreviewOrbitCameraState& state,
    const Vec2& mouseDelta,
    float mouseSensor)
{
    state.yawDegrees += mouseDelta.x * mouseSensor;
    state.pitchDegrees = clamp(state.pitchDegrees + mouseDelta.y * mouseSensor, -89.f, 89.f);
}

void EffectPreviewOrbitCamera::Pan(
    EffectPreviewOrbitCameraState& state,
    const Vec2& mouseDelta,
    const Vec3& right,
    const Vec3& up,
    float panScale)
{
    state.pivot += (-right * mouseDelta.x + up * mouseDelta.y) * panScale;
}

void EffectPreviewOrbitCamera::Dolly(
    EffectPreviewOrbitCameraState& state,
    float delta,
    float dollyScale,
    float minDistance,
    float maxDistance)
{
    state.distance = clamp(state.distance - delta * dollyScale, minDistance, maxDistance);
}

EffectPreviewOrbitCameraFrame EffectPreviewOrbitCamera::Build_Frame(
    const EffectPreviewOrbitCameraState& state,
    float aspect,
    float fovYRadians,
    float nearPlane,
    float farPlane)
{
    EffectPreviewOrbitCameraFrame frame{};

    const float yawRadians = XMConvertToRadians(state.yawDegrees);
    const float pitchRadians = XMConvertToRadians(state.pitchDegrees);

    Vec3 backDir;
    backDir.x = -sinf(yawRadians);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRadians);
    backDir.Normalize();

    Vec3 rightDir;
    rightDir.x = cosf(yawRadians);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRadians);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * max(state.distance, 0.1f);
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRadians);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    frame.eye = state.pivot + orbitOffset;
    frame.right = rightDir;
    frame.view = XMMatrixLookAtLH(
        XMVectorSet(frame.eye.x, frame.eye.y, frame.eye.z, 1.f),
        XMVectorSet(state.pivot.x, state.pivot.y, state.pivot.z, 1.f),
        XMVectorSet(0.f, 1.f, 0.f, 0.f)
    );
    frame.proj = XMMatrixPerspectiveFovLH(
        fovYRadians,
        max(aspect, 0.0001f),
        nearPlane,
        farPlane
    );

    return frame;
}

NS_END
