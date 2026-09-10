#pragma once

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Client)

class EffectCom;

class TrailSampleProvider_ByPart final : public IEffectTrailSampleProvider
{
public:
    TrailSampleProvider_ByPart(
        const Weak<EffectCom>& effectCom,
        wstring partTag,
        const TrailAnchorAsset& trailAnchorAsset,
        uint64 trailNotifyToken = 0,
        Shared<bool> sampleEnabled = nullptr);

    ~TrailSampleProvider_ByPart() override = default;

public:
    bool Try_GetTrailSample(EffectTrailSample& outSample) const override;

private:
    Weak<EffectCom> _effectCom{};
    wstring _partTag{};
    TrailAnchorAsset _trailAnchorAsset{};
    uint64 _trailNotifyToken = 0;
    Shared<bool> _sampleEnabled{};
};

NS_END
