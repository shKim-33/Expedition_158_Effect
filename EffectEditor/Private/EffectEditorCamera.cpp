#include "pch.h"
#include "EffectEditorCamera.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Helper_Math.h"

NS_BEGIN(EffectEditor)

namespace
{
    bool Is_PreviewCameraDragInputHeld()
    {
        const bool altHeld =
            GAME->KeyPress(KEY_TYPE::ALT) || GAME->KeyDown(KEY_TYPE::ALT);

        return GAME->KeyPress(KEY_TYPE::RBUTTON) ||
               (altHeld &&
                (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                 ImGui::IsMouseDown(ImGuiMouseButton_Middle)));
    }
}

EffectEditorCamera::EffectEditorCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

EffectEditorCamera::EffectEditorCamera(const EffectEditorCamera& prototype)
    : Camera{ prototype }
    , _targetPivot{ prototype._targetPivot }
    , _focusSpeed{ prototype._focusSpeed }
    , _orbitState{ prototype._orbitState }
    , _homePivot{ prototype._homePivot }
    , _homeDistance{ prototype._homeDistance }
    , _homeYaw{ prototype._homeYaw }
    , _homePitch{ prototype._homePitch }
{
}

EffectEditorCamera::~EffectEditorCamera()
{
    Free();
}

HRESULT EffectEditorCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT EffectEditorCamera::Initialize(void* arg)
{
    if (nullptr == arg)
        return E_FAIL;

    const auto desc = static_cast<EffectEditorCameraDesc*>(arg);

    _mouseSensor = desc->mouseSensor;
    _orbitState.pivot = desc->pivot;
    _orbitState.distance = desc->distance;
    _targetPivot = _orbitState.pivot;

    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    const Vec3 eye = Vec3(desc->eye.x, desc->eye.y, desc->eye.z);
    _homePivot = _orbitState.pivot;
    _homeDistance = max((eye - _homePivot).Length(), 0.1f);

    _transformCom->Set_WorldPosition(eye);
    _transformCom->LookAt(_homePivot);

    const Vec3 rotationEuler = _transformCom->Get_WorldRotationEuler();
    _homePitch = rotationEuler.x;
    _homeYaw = rotationEuler.y;

    if (_orbitState.distance <= 0.01f)
        EffectPreviewOrbitCamera::Reset(_orbitState, _homePivot, _homeDistance, _homeYaw, _homePitch);
    else
    {
        _orbitState.yawDegrees = _homeYaw;
        _orbitState.pitchDegrees = _homePitch;
    }

    Update_CameraTransform();
    Update_TransformMatrices();

    Set_Name(L"EffectEditor Camera");

    return S_OK;
}

void EffectEditorCamera::BeginPlay()
{
    Camera::BeginPlay();

    CHECK_FAILED(GAME->Add_Camera(GetSharedPtr<Camera>()));
}

void EffectEditorCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    const float cameraDelta = max(timeDelta, GAME->Get_FreeCameraTimeDelta());

    if (_hasPendingFocus)
    {
        _orbitState.pivot = Math::Lerp_Damp(_orbitState.pivot, _targetPivot, _focusSpeed, cameraDelta);
        if ((_targetPivot - _orbitState.pivot).LengthSquared() <= 0.001f)
        {
            _orbitState.pivot = _targetPivot;
            _hasPendingFocus = false;
        }
    }

    if (_hasPendingHome)
    {
        _orbitState.pivot = Math::Lerp_Damp(_orbitState.pivot, _homePivot, _focusSpeed, cameraDelta);
        _orbitState.distance = Math::Lerp_Damp(_orbitState.distance, _homeDistance, _focusSpeed, cameraDelta);
        _orbitState.yawDegrees = Math::Lerp_Damp(_orbitState.yawDegrees, _homeYaw, _focusSpeed, cameraDelta);
        _orbitState.pitchDegrees = Math::Lerp_Damp(_orbitState.pitchDegrees, _homePitch, _focusSpeed, cameraDelta);

        if ((_homePivot - _orbitState.pivot).LengthSquared() <= 0.001f &&
            fabsf(_homeDistance - _orbitState.distance) <= 0.01f &&
            fabsf(_homeYaw - _orbitState.yawDegrees) <= 0.01f &&
            fabsf(_homePitch - _orbitState.pitchDegrees) <= 0.01f)
        {
            EffectPreviewOrbitCamera::Reset(_orbitState, _homePivot, _homeDistance, _homeYaw, _homePitch);
            _hasPendingHome = false;
        }
    }

    if (!EDITOR->Is_PreviewCameraControlEnabled())
    {
        EDITOR->End_PreviewCameraDrag();
        Update_CameraTransform();
        Update_TransformMatrices();
        return;
    }

    if (EDITOR->Is_PreviewCameraDragging() && !Is_PreviewCameraDragInputHeld())
        EDITOR->End_PreviewCameraDrag();

    if (ImGui::IsKeyPressed(ImGuiKey_F, false))
        Start_HomeTransition();

    const bool altHeld =
        GAME->KeyPress(KEY_TYPE::ALT) || GAME->KeyDown(KEY_TYPE::ALT);
    const bool panDragging = altHeld && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool orbitDragging = altHeld && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool rightMouseHeld = EDITOR->Is_PreviewCameraDragging() && GAME->KeyPress(KEY_TYPE::RBUTTON);
    const bool dollyDragging = altHeld && rightMouseHeld;
    const bool freeFlyDragging = !altHeld && rightMouseHeld;
    const bool isDragging = panDragging || orbitDragging || dollyDragging || freeFlyDragging;
    const float wheelDelta = GAME->Get_MouseWheel();

    if (rightMouseHeld && wheelDelta != 0.f)
    {
        _hasPendingFocus = false;
        _hasPendingHome = false;

        const float wheelStep = wheelDelta / static_cast<float>(WHEEL_DELTA);
        const float wheelDollyScale = max(0.1f, _orbitState.distance * 0.13f);
        EffectPreviewOrbitCamera::Dolly(_orbitState, wheelStep, wheelDollyScale, 0.5f, _far);

        Update_CameraTransform();
        Update_TransformMatrices();
        return;
    }

    if (panDragging || orbitDragging || dollyDragging)
    {
        _hasPendingFocus = false;
        _hasPendingHome = false;
        _targetPivot = _orbitState.pivot;

        const Vec2 mouseDelta = GAME->Get_MouseDelta();

        if (panDragging)
        {
            const float panScale = max(0.01f, _orbitState.distance * 0.0025f);
            EffectPreviewOrbitCamera::Pan(
                _orbitState,
                mouseDelta,
                _transformCom->Get_WorldRight(),
                _transformCom->Get_WorldUp(),
                panScale
            );
            _targetPivot = _orbitState.pivot;
        }
        else if (orbitDragging)
            EffectPreviewOrbitCamera::Orbit(_orbitState, mouseDelta, _mouseSensor);
        else if (dollyDragging)
        {
            const float dollyScale = max(0.02f, _orbitState.distance * 0.0046f);
            EffectPreviewOrbitCamera::Dolly(_orbitState, mouseDelta.x, dollyScale, 0.5f, _far);
        }

        Update_CameraTransform();
        Update_TransformMatrices();
        return;
    }

    if (freeFlyDragging)
    {
        _hasPendingFocus = false;
        _hasPendingHome = false;

        if (!rightMouseHeld && GAME->KeyPress(KEY_TYPE::CTRL) && wheelDelta != 0.f)
        {
            float moveSpeed = _transformCom->Get_SpeedPerSec();

            constexpr float step = 2.f;
            if (wheelDelta > 0.f)
                moveSpeed += step;
            else
                moveSpeed -= step;

            moveSpeed = clamp(moveSpeed, 0.f, 100.f);
            _transformCom->Set_SpeedPerSec(moveSpeed);
        }

        if (GAME->KeyPress(KEY_TYPE::W))
            _transformCom->Go_Straight(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::S))
            _transformCom->Go_Backward(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::A))
            _transformCom->Go_Left(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::D))
            _transformCom->Go_Right(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::E))
            _transformCom->Go_Up(cameraDelta);

        if (GAME->KeyPress(KEY_TYPE::Q))
            _transformCom->Go_Down(cameraDelta);

        const Vec2 mouseDelta = GAME->Get_MouseDelta();
        _yaw += mouseDelta.x * _mouseSensor;
        _pitch += mouseDelta.y * _mouseSensor;

        if (0.f != mouseDelta.x || 0.f != mouseDelta.y)
        {
            _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));
            _orbitState.yawDegrees = _yaw;
            _orbitState.pitchDegrees = _pitch;
        }

        Sync_PivotFromCurrentView();
        Update_TransformMatrices();
        return;
    }

    Update_CameraTransform();
    Update_TransformMatrices();
}

void EffectEditorCamera::Update(float)
{
}

void EffectEditorCamera::Late_Update(float)
{
}

HRESULT EffectEditorCamera::Render()
{
    return S_OK;
}

void EffectEditorCamera::Request_Focus(const Vec3& pivot)
{
    _targetPivot = pivot;
    _hasPendingFocus = true;
    _hasPendingHome = false;
}

void EffectEditorCamera::Sync_PivotFromCurrentView()
{
    _orbitState.distance = max(_orbitState.distance, 0.1f);

    const Vec3 worldForward = _transformCom->Get_WorldForward();
    if (worldForward.LengthSquared() <= 0.0001f)
        return;

    _orbitState.pivot = _transformCom->Get_WorldPosition() + worldForward * _orbitState.distance;
    _targetPivot = _orbitState.pivot;
}

void EffectEditorCamera::Start_HomeTransition()
{
    _hasPendingFocus = false;
    _hasPendingHome = true;
    _targetPivot = _homePivot;
}

void EffectEditorCamera::Update_CameraTransform()
{
    _orbitState.distance = max(_orbitState.distance, 0.1f);

    const EffectPreviewOrbitCameraFrame frame = EffectPreviewOrbitCamera::Build_Frame(
        _orbitState,
        max(_aspect, 0.0001f),
        _fovY,
        _near,
        _far
    );

    _yaw = _orbitState.yawDegrees;
    _pitch = _orbitState.pitchDegrees;
    _transformCom->Set_WorldPosition(frame.eye);
    _transformCom->LookAt(_orbitState.pivot);
}

Shared<EffectEditorCamera> EffectEditorCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectEditorCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : EffectEditorCamera");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectEditorCamera::Clone(void* arg)
{
    auto instance = make_shared<EffectEditorCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : EffectEditorCamera");
        return nullptr;
    }

    return instance;
}

void EffectEditorCamera::Free()
{
    __super::Free();
}

NS_END
