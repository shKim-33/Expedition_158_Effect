#pragma once

#include "AnimNotify.h"
#include "EffectCom.h"

NS_BEGIN(Client)

class AN_PlayEffect : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_PlayEffect)

public:
    AN_PlayEffect() = default;
    ~AN_PlayEffect() override = default;

public:
    string Get_TypeName() const override { return "AN_PlayEffect"; }
    void Execute(const AnimNotifyContext& context) override;

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
};

NS_END
