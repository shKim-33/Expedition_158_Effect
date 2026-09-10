#include "SourcePointSampleProvider_ByPart.h"

#include "EffectCom.h"

NS_BEGIN(Client)

SourcePointSampleProvider_ByPart::SourcePointSampleProvider_ByPart(
    const Weak<EffectCom>& effectCom,
    wstring partTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    string socketName,
    string sourceHistoryAnchorName,
    Vec3 sourceHistoryLocalOffset,
    Shared<bool> sampleEnabled)
    : _effectCom(effectCom)
    , _partTag(move(partTag))
    , _sourceBindingMode(sourceBindingMode)
    , _socketName(move(socketName))
    , _sourceHistoryAnchorName(move(sourceHistoryAnchorName))
    , _sourceHistoryLocalOffset(sourceHistoryLocalOffset)
    , _sampleEnabled(move(sampleEnabled))
{
}

bool SourcePointSampleProvider_ByPart::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    if (nullptr != _sampleEnabled && !*_sampleEnabled)
        return false;

    const Shared<EffectCom> effectCom = _effectCom.lock();
    if (nullptr == effectCom)
        return false;

    return effectCom->Try_GetSourcePointSampleForPart(
        _partTag,
        _sourceBindingMode,
        _socketName,
        _sourceHistoryAnchorName,
        _sourceHistoryLocalOffset,
        outSample
    );
}

NS_END
