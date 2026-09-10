#pragma once

#include "AnimNotifyState.h"
#include "Flash_ScreenFx.h"
#include "ScreenFx_Manager.h"
#include "ScreenFxCueTypes.h"

NS_BEGIN(Client)

class ANS_ScreenFxCue : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_ScreenFxCue)

public:
    ANS_ScreenFxCue() = default;
    ~ANS_ScreenFxCue() override = default;

public:
    string Get_TypeName() const override { return "ANS_ScreenFxCue"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext&) override;
    void On_End(const AnimNotifyContext& context) override;
    void From_Json(const json& data) override;

private:
    ScreenFxCueType _cueType{ ScreenFxCueType::Flash };
    ScreenFxExclusionMode _exclusionMode{ ScreenFxExclusionMode::None };
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };
    float _intensity{ 1.f };
    float _desaturateAmount{ 1.f };
    float _fadeInRatio{ 0.f };
    float _fadeOutRatio{ 0.5f };
    ScreenFxShapeMode _shapeMode{ ScreenFxShapeMode::FullScreen };
    Vec2 _center{ 0.5f, 0.5f };
    float _startRadius{ 0.f };
    float _endRadius{ 1.f };
    float _softness{ 0.05f };
    bool _invertShape{ false };
    Engine::ScreenFxHandle _activeScreenFxHandle{};
};

NS_END
