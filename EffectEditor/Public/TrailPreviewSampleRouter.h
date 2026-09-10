#pragma once

#include "EffectRuntime_Types.h"

NS_BEGIN(EffectEditor)

class TrailPreviewSampleRouter final : public IEffectTrailSampleProvider, public IEffectSourcePointSampleProvider
{
public:
    void Bind_PlayerProviders(
        const Weak<IEffectTrailSampleProvider>& trailProvider,
        const Weak<IEffectSourcePointSampleProvider>& sourceProvider);
    void Bind_MonsterProviders(
        const Weak<IEffectTrailSampleProvider>& trailProvider,
        const Weak<IEffectSourcePointSampleProvider>& sourceProvider);

    bool Try_GetTrailSample(EffectTrailSample& outSample) const override;
    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override;

private:
    Weak<IEffectTrailSampleProvider> Resolve_TrailProvider() const;
    Weak<IEffectSourcePointSampleProvider> Resolve_SourceProvider() const;

private:
    Weak<IEffectTrailSampleProvider> _playerTrailProvider{};
    Weak<IEffectSourcePointSampleProvider> _playerSourceProvider{};
    Weak<IEffectTrailSampleProvider> _monsterTrailProvider{};
    Weak<IEffectSourcePointSampleProvider> _monsterSourceProvider{};
};

NS_END
