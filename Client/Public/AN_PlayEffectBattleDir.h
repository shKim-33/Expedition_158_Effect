#pragma once

#include "AnimNotify.h"
#include "EffectCom.h"

NS_BEGIN(Client)

class AN_PlayEffectBattleDir : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_PlayEffectBattleDir)

public:
    AN_PlayEffectBattleDir() = default;
    ~AN_PlayEffectBattleDir() override = default;

public:
    string Get_TypeName() const override { return "AN_PlayEffectBattleDir"; }
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
};

NS_END
