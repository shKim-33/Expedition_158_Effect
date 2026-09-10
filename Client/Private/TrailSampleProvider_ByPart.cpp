#include "TrailSampleProvider_ByPart.h"

#include "EffectCom.h"

NS_BEGIN(Client)

TrailSampleProvider_ByPart::TrailSampleProvider_ByPart(
    const Weak<EffectCom>& effectCom,
    wstring partTag,
    const TrailAnchorAsset& trailAnchorAsset,
    uint64 trailNotifyToken,
    Shared<bool> sampleEnabled)
    : _effectCom(effectCom)
    , _partTag(move(partTag))
    , _trailAnchorAsset(trailAnchorAsset)
    , _trailNotifyToken(trailNotifyToken)
    , _sampleEnabled(move(sampleEnabled))
{
}

bool TrailSampleProvider_ByPart::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    if (nullptr != _sampleEnabled && !*_sampleEnabled)
        return false;

    const Shared<EffectCom> effectCom = _effectCom.lock();
    if (nullptr == effectCom)
        return false;

    return effectCom->Try_GetTrailSampleForPart(_partTag, _trailAnchorAsset, _trailNotifyToken, outSample);
}

NS_END
