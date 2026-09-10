#pragma once

NS_BEGIN(EffectEditor)

struct EffectPreviewOrbitCameraState
{
    Vec3 pivot{ Vec3::Zero };
    float distance{ 4.2f };
    float yawDegrees{ 0.f };
    float pitchDegrees{ 0.f };
};

struct EffectPreviewOrbitCameraFrame
{
    Vec3 eye{ Vec3::Zero };
    Vec3 right{ Vec3::Right };
    Matrix view{ Matrix::Identity };
    Matrix proj{ Matrix::Identity };
};

class EffectPreviewOrbitCamera final
{
public:
    static void Reset(
        EffectPreviewOrbitCameraState& state,
        const Vec3& pivot,
        float distance,
        float yawDegrees,
        float pitchDegrees);

    static void Orbit(
        EffectPreviewOrbitCameraState& state,
        const Vec2& mouseDelta,
        float mouseSensor);

    static void Pan(
        EffectPreviewOrbitCameraState& state,
        const Vec2& mouseDelta,
        const Vec3& right,
        const Vec3& up,
        float panScale);

    static void Dolly(
        EffectPreviewOrbitCameraState& state,
        float delta,
        float dollyScale,
        float minDistance,
        float maxDistance);

    static EffectPreviewOrbitCameraFrame Build_Frame(
        const EffectPreviewOrbitCameraState& state,
        float aspect,
        float fovYRadians,
        float nearPlane,
        float farPlane);
};

NS_END
