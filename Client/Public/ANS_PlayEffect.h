#pragma once

#include "AnimNotifyState.h"
#include "EffectCom.h"

NS_BEGIN(Client)

class ANS_PlayEffect : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_PlayEffect)

public:
    ANS_PlayEffect() = default;
    ~ANS_PlayEffect() override = default;

public:
    string Get_TypeName() const override { return "ANS_PlayEffect"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    wstring                     _effectName = L"";
    EffectCom::EffectPlayTarget _target = EffectCom::EffectPlayTarget::OwnerRoot;
    wstring                     _partTag = L"";
    string                      _socketName = "";
    wstring                     _layerTag = L"Layer_Effect";
    Vec3                        _localOffset = Vec3::Zero;
    Vec3                        _localRotation = Vec3::Zero;
    Vec3                        _scale = Vec3::One;
    float                       _playbackSpeed = 1.f;
    bool                        _inheritPosition = true;
    bool                        _inheritRotation = true;
    bool                        _inheritScale = false;
    EffectInstancePool::AttachedHandle _activeEffectHandle{};
};

NS_END
