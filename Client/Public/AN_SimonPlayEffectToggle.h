#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_SimonPlayEffectToggle : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SimonPlayEffectToggle)

public:
    AN_SimonPlayEffectToggle() = default;
    ~AN_SimonPlayEffectToggle() override = default;

public:
    string Get_TypeName() const override { return "AN_SimonPlayEffectToggle"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    bool _playEffect = true;
};

NS_END
