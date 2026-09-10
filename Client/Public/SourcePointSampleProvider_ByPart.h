#pragma once

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

class EffectCom;

enum class SourceHistorySourceBindingMode
{
    Socket = 1,
    WeaponAnchor = 2,
    TrailAnchor = 3,
};

class SourcePointSampleProvider_ByPart final : public IEffectSourcePointSampleProvider
{
public:
    SourcePointSampleProvider_ByPart(
        const Weak<EffectCom>& effectCom,
        wstring partTag,
        SourceHistorySourceBindingMode sourceBindingMode,
        string socketName,
        string sourceHistoryAnchorName,
        Vec3 sourceHistoryLocalOffset,
        Shared<bool> sampleEnabled = nullptr);
    ~SourcePointSampleProvider_ByPart() override = default;

public:
    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override;

private:
    Weak<EffectCom> _effectCom{};
    wstring _partTag{};
    SourceHistorySourceBindingMode _sourceBindingMode{ SourceHistorySourceBindingMode::WeaponAnchor };
    string _socketName{};
    string _sourceHistoryAnchorName{ "Source" };
    Vec3 _sourceHistoryLocalOffset{ Vec3::Zero };
    Shared<bool> _sampleEnabled{};
};

NS_END
