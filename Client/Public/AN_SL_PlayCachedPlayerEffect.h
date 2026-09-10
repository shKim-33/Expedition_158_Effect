#pragma once

#include "AnimNotify.h"

NS_BEGIN(Engine)
class ContainerObject;
NS_END

NS_BEGIN(Client)

class SL_Dualliste;

class AN_SL_PlayCachedPlayerEffect final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_SL_PlayCachedPlayerEffect)

public:
    AN_SL_PlayCachedPlayerEffect() = default;
    ~AN_SL_PlayCachedPlayerEffect() override = default;

public:
    string Get_TypeName() const override { return "AN_SL_PlayCachedPlayerEffect"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    Shared<SL_Dualliste> Resolve_OwnerDualliste(GameObject* notifyOwner) const;

private:
    wstring _effectName = L"";
    wstring _layerTag = L"Layer_Effect";
    Vec3 _localOffset = Vec3::Zero;
    Vec3 _scale = Vec3::One;
};

NS_END
