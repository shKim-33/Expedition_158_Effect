#include "TrailPreviewSampleRouter.h"

#include "EffectEditorInstance.h"

NS_BEGIN(EffectEditor)

void TrailPreviewSampleRouter::Bind_PlayerProviders(
    const Weak<IEffectTrailSampleProvider>& trailProvider,
    const Weak<IEffectSourcePointSampleProvider>& sourceProvider)
{
    _playerTrailProvider = trailProvider;
    _playerSourceProvider = sourceProvider;
}

void TrailPreviewSampleRouter::Bind_MonsterProviders(
    const Weak<IEffectTrailSampleProvider>& trailProvider,
    const Weak<IEffectSourcePointSampleProvider>& sourceProvider)
{
    _monsterTrailProvider = trailProvider;
    _monsterSourceProvider = sourceProvider;
}

bool TrailPreviewSampleRouter::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    if (EDITOR == nullptr || !EDITOR->Is_TrailPreviewVisible())
        return false;

    const Shared<IEffectTrailSampleProvider> provider = Resolve_TrailProvider().lock();
    if (provider == nullptr)
        return false;

    return provider->Try_GetTrailSample(outSample);
}

bool TrailPreviewSampleRouter::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    if (EDITOR == nullptr || !EDITOR->Is_TrailPreviewVisible())
        return false;

    const Shared<IEffectSourcePointSampleProvider> provider = Resolve_SourceProvider().lock();
    if (provider == nullptr)
        return false;

    return provider->Try_GetSourcePointSample(outSample);
}

Weak<IEffectTrailSampleProvider> TrailPreviewSampleRouter::Resolve_TrailProvider() const
{
    if (EDITOR == nullptr)
        return {};

    switch (EDITOR->Get_TrailPreviewCharacterSlot())
    {
    case TrailPreviewCharacterSlot::Monster:
        return _monsterTrailProvider;
    case TrailPreviewCharacterSlot::Player:
    default:
        return _playerTrailProvider;
    }
}

Weak<IEffectSourcePointSampleProvider> TrailPreviewSampleRouter::Resolve_SourceProvider() const
{
    if (EDITOR == nullptr)
        return {};

    switch (EDITOR->Get_TrailPreviewCharacterSlot())
    {
    case TrailPreviewCharacterSlot::Monster:
        return _monsterSourceProvider;
    case TrailPreviewCharacterSlot::Player:
    default:
        return _playerSourceProvider;
    }
}

NS_END
