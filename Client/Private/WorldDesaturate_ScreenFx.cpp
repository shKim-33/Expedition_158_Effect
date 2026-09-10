#include "pch.h"
#include "WorldDesaturate_ScreenFx.h"

#include "ClientScreenFxManager.h"
#include "Shader_CBuffer_Define.h"
#include "ShaderCom.h"

NS_BEGIN(Client)

REGISTER_GLOBAL_ENUM(Client::ScreenFxExclusionMode, "ScreenFxExclusionMode")

IMPLEMENT_REFLECTION(WorldDesaturate_ScreenFx)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "World Desaturate";
    info.type = RegisteredType::ScreenFx;

    PROPERTY_FLOAT("Amount", _originalDesc.amount, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Duration", _originalDesc.duration, 0.01f, 10.f, 0.01f);
    PROPERTY_FLOAT("Fade In Time", _originalDesc.fadeInTime, 0.f, 10.f, 0.01f);
    PROPERTY_FLOAT("Fade Out Time", _originalDesc.fadeOutTime, 0.f, 10.f, 0.01f);
    PROPERTY_ENUM("Shape Mode", _originalDesc.shapeMode, ScreenFxShapeMode);
    PROPERTY_VEC2("Center", _originalDesc.center);
    PROPERTY_FLOAT("Start Radius", _originalDesc.startRadius, 0.f, 2.f, 0.01f);
    PROPERTY_FLOAT("End Radius", _originalDesc.endRadius, 0.f, 2.f, 0.01f);
    PROPERTY_FLOAT("Softness", _originalDesc.softness, 0.001f, 1.f, 0.001f);
    PROPERTY_BOOL("Invert Shape", _originalDesc.invertShape);
    PROPERTY_ENUM("Exclusion Mode", _originalDesc.exclusionMode, ScreenFxExclusionMode);
    PROPERTY_FLOAT("Elapsed Time", _elapsedTime, 0.f, 10.f, 0.01f);
    return true;
}

HRESULT WorldDesaturate_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    if (desc)
    {
        if (FAILED(ScreenFx_Instance::Initialize(desc)))
            return E_FAIL;

        const DesaturateDesc* desaturateDesc = static_cast<DesaturateDesc*>(desc);
        _originalDesc = *desaturateDesc;
    }
    else
    {
        _duration = _originalDesc.duration;
        _elapsedTime = 0.f;
    }

    _passType = ETOI(_originalDesc.exclusionMode == ScreenFxExclusionMode::MarkedObjects
        ? ClientScreenFxManager::ScreenFx_Type::WorldDesaturateExclude
        : ClientScreenFxManager::ScreenFx_Type::WorldDesaturate);
    Evaluate_At(_elapsedTime);
    return S_OK;
}

void WorldDesaturate_ScreenFx::Update_ScreenFx(float timeDelta)
{
    _elapsedTime += timeDelta;
    Evaluate_At(_elapsedTime);
}

HRESULT WorldDesaturate_ScreenFx::Bind_FxConstantBuffer(ShaderCom* shader)
{
    Engine::ScreenFxCB cbData{};

    cbData.screenFxShape = Vec4(
        _originalDesc.center.x,
        _originalDesc.center.y,
        _currentRadius,
        max(0.001f, _originalDesc.softness));
    cbData.screenFxParams = Vec4(
        _currentAmount,
        static_cast<float>(static_cast<int32>(_originalDesc.shapeMode)),
        _originalDesc.invertShape ? 1.f : 0.f,
        0.f);

    return shader->Bind_CBufferData(cbData);
}

void WorldDesaturate_ScreenFx::Evaluate_At(float elapsedTime)
{
    _elapsedTime = max(0.f, elapsedTime);
    _duration = max(0.f, _originalDesc.duration);

    const float duration = max(0.001f, _duration);
    const float normalizedTime = std::clamp(_elapsedTime / duration, 0.f, 1.f);
    _currentRadius = std::lerp(_originalDesc.startRadius, _originalDesc.endRadius, normalizedTime);
    _currentAmount = std::clamp(Compute_Envelope(_elapsedTime) * _originalDesc.amount, 0.f, 1.f);
}

float WorldDesaturate_ScreenFx::Compute_Envelope(float elapsedTime) const
{
    if (_originalDesc.duration <= 0.f || elapsedTime >= _originalDesc.duration)
        return 0.f;

    float envelope = 1.f;

    if (_originalDesc.fadeInTime > 0.f && elapsedTime < _originalDesc.fadeInTime)
        envelope = min(envelope, elapsedTime / _originalDesc.fadeInTime);

    const float remainingTime = _originalDesc.duration - elapsedTime;
    if (_originalDesc.fadeOutTime > 0.f && remainingTime < _originalDesc.fadeOutTime)
        envelope = min(envelope, remainingTime / _originalDesc.fadeOutTime);

    return std::clamp(envelope, 0.f, 1.f);
}

Shared<WorldDesaturate_ScreenFx> WorldDesaturate_ScreenFx::Create(DesaturateDesc* desc)
{
    auto instance = make_shared<WorldDesaturate_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}

NS_END
