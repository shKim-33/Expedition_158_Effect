#pragma once

#include "AnimNotifyState.h"
#include "SourcePointSampleProvider_ByPart.h"

NS_BEGIN(Client)

class ANS_SourceHistoryEffect : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SourceHistoryEffect)

public:
    ANS_SourceHistoryEffect() = default;
    ~ANS_SourceHistoryEffect() override = default;

public:
    string Get_TypeName() const override { return "ANS_SourceHistoryEffect"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;
    void From_Json(const json& data) override;

private:
    wstring _effectName = L"";
    wstring _partTag = L"Part_Weapon";
    wstring _layerTag = L"Layer_Effect";
    SourceHistorySourceBindingMode _sourceBindingMode{ SourceHistorySourceBindingMode::WeaponAnchor };
    string _socketName{};
    string _sourceHistoryAnchorName{ "Source" };
    Vec3 _sourceHistoryLocalOffset{ Vec3::Zero };
};

NS_END
