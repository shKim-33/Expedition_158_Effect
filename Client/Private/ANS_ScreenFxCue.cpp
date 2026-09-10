#include "ANS_ScreenFxCue.h"

#include "Flash_ScreenFx.h"
#include "GameInstance.h"
#include "WorldDesaturate_ScreenFx.h"

REGISTER_GLOBAL_ENUM(Client::ScreenFxCueType, "ScreenFxCueType")

IMPLEMENT_REFLECTION(ANS_ScreenFxCue)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("Cue Type", _cueType, ScreenFxCueType);
    PROPERTY_ENUM("Exclusion Mode", _exclusionMode, ScreenFxExclusionMode);
    PROPERTY_COLOR("Color", _color);
    PROPERTY_FLOAT("Intensity", _intensity, 0.f, 10.f, 0.01f);
    PROPERTY_FLOAT("Desaturate Amount", _desaturateAmount, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Fade In Ratio", _fadeInRatio, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Fade Out Ratio", _fadeOutRatio, 0.f, 1.f, 0.01f);
    PROPERTY_ENUM("Shape Mode", _shapeMode, ScreenFxShapeMode);
    PROPERTY_VEC2("Center", _center);
    PROPERTY_FLOAT("Start Radius", _startRadius, 0.f, 2.f, 0.01f);
    PROPERTY_FLOAT("End Radius", _endRadius, 0.f, 2.f, 0.01f);
    PROPERTY_FLOAT("Softness", _softness, 0.001f, 1.f, 0.001f);
    PROPERTY_BOOL("Invert Shape", _invertShape);

    return true;
}

void ANS_ScreenFxCue::On_Begin(const AnimNotifyContext& context)
{
    if (_activeScreenFxHandle.Is_Valid())
    {
        if (context.isPreview)
            GAME->Stop_PreviewScreenFx(_activeScreenFxHandle);
        else
            GAME->Stop_ScreenFx(_activeScreenFxHandle);
    }

    _activeScreenFxHandle = {};

    const float duration = max(0.f, context.notifyStateDurationSec);
    if (duration <= 0.f)
        return;

    const float fadeInRatio = std::clamp(_fadeInRatio, 0.f, 1.f);
    const float fadeOutRatio = std::clamp(_fadeOutRatio, 0.f, 1.f);

    Shared<ScreenFx_Instance> screenFx{};

    if (_cueType == ScreenFxCueType::WorldDesaturate)
    {
        WorldDesaturate_ScreenFx::DesaturateDesc desc{};
        desc.duration = duration;
        desc.amount = std::clamp(_desaturateAmount, 0.f, 1.f);
        desc.fadeInTime = duration * fadeInRatio;
        desc.fadeOutTime = duration * fadeOutRatio;
        desc.shapeMode = _shapeMode;
        desc.center = _center;
        desc.startRadius = max(0.f, _startRadius);
        desc.endRadius = max(0.f, _endRadius);
        desc.softness = max(0.001f, _softness);
        desc.invertShape = _invertShape;
        desc.exclusionMode = _exclusionMode;

        screenFx = WorldDesaturate_ScreenFx::Create(&desc);
    }
    else
    {
        Flash_ScreenFx::FlashDesc desc{};
        desc.duration = duration;
        desc.color = _color;
        desc.intensity = max(0.f, _intensity);
        desc.fadeInTime = duration * fadeInRatio;
        desc.fadeOutTime = duration * fadeOutRatio;
        desc.shapeMode = _shapeMode;
        desc.center = _center;
        desc.startRadius = max(0.f, _startRadius);
        desc.endRadius = max(0.f, _endRadius);
        desc.softness = max(0.001f, _softness);
        desc.invertShape = _invertShape;
        desc.exclusionMode = _exclusionMode;

        screenFx = Flash_ScreenFx::Create(&desc);
    }

    if (screenFx == nullptr)
        return;

    const float progress = std::clamp(context.notifyStateProgress, 0.f, 1.f);
    screenFx->Evaluate_At(duration * progress);

    _activeScreenFxHandle = context.isPreview
        ? GAME->Play_PreviewScreenFx_WithHandle(screenFx)
        : GAME->Play_ScreenFx_WithHandle(screenFx);
}

void ANS_ScreenFxCue::On_Tick(const AnimNotifyContext& context)
{
    if (!context.isPreview || !_activeScreenFxHandle.Is_Valid())
        return;

    const float duration = max(0.f, context.notifyStateDurationSec);
    if (duration <= 0.f)
        return;

    const float progress = std::clamp(context.notifyStateProgress, 0.f, 1.f);
    GAME->Evaluate_PreviewScreenFx(_activeScreenFxHandle, duration * progress);
}

void ANS_ScreenFxCue::On_End(const AnimNotifyContext& context)
{
    if (!_activeScreenFxHandle.Is_Valid())
        return;

    if (context.isPreview)
        GAME->Stop_PreviewScreenFx(_activeScreenFxHandle);
    else
        GAME->Stop_ScreenFx(_activeScreenFxHandle);

    _activeScreenFxHandle = {};
}

void ANS_ScreenFxCue::From_Json(const json& data)
{
    AnimNotifyState::From_Json(data);

    if (!data.contains("_exclusionMode") &&
        data.value("_flashExcludeMarkedObjects", false))
    {
        _exclusionMode = ScreenFxExclusionMode::MarkedObjects;
    }
}
