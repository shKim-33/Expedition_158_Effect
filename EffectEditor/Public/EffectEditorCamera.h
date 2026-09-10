#pragma once

#include "Camera.h"
#include "EffectPreviewOrbitCamera.h"

NS_BEGIN(EffectEditor)

class EffectEditorCamera final : public Camera
{
public:
    struct EffectEditorCameraDesc final : public CAMERA_DESC
    {
        Vec3 pivot{ Vec3::Zero };
        float distance{ 0.f };
    };

public:
    EffectEditorCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectEditorCamera(const EffectEditorCamera& prototype);
    ~EffectEditorCamera() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public: //## Behavior::CameraControl
    void Request_Focus(const Vec3& pivot);
    const Vec3& Get_Pivot() const { return _orbitState.pivot; }

private: //## Data::Focus
    Vec3 _targetPivot{ Vec3::Zero };
    float _focusSpeed{ 10.f };
    bool _hasPendingFocus{ false };

private: //## Data::Orbit
    EffectPreviewOrbitCameraState _orbitState{};

private: //## Data::Home
    Vec3 _homePivot{ Vec3::Zero };
    float _homeDistance{ 0.f };
    float _homeYaw{ 0.f };
    float _homePitch{ 0.f };
    bool _hasPendingHome{ false };

private: //## Helper::Focus
    void Sync_PivotFromCurrentView();

private: //## Helper::Home
    void Start_HomeTransition();

private: //## Helper::Transform
    void Update_CameraTransform();

public:
    static Shared<EffectEditorCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
