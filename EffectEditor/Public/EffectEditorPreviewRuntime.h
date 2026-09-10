#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectInstancePool.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(EffectEditor)

class TrailPreviewController;

class EffectEditorPreviewRuntime final
{
public:
    HRESULT Initialize(const wstring& layerTag);
    void Bind_TrailPreviewSources(
        const Weak<IEffectTrailSampleProvider>& trailSampleProvider,
        const Weak<IEffectSourcePointSampleProvider>& sourcePointSampleProvider,
        const Weak<TrailPreviewController>& trailPreviewController);
    void Update(float timeDelta);

    HRESULT Restart_FromAuthoring(const vector<AuthoringEmitter>& emitters, const HistoryBudgetData& historyBudget);
    HRESULT Reset_Runtime();
    bool Is_Finished() const;
    void Apply_PreviewEmitterTransformOverrides(const vector<AuthoringEmitter>& emitters);
    bool Try_GetPreviewEmitterWorldMatrix(uint32 emitterId, Matrix& outWorldMatrix) const;

    void Clear();

private: //## Types::TrailPreview
    struct PreviewTrailStrokeRuntime
    {
        EffectInstancePool::AttachedHandle poolHandle{};
        Shared<bool> sampleEnabled{};
        Shared<IEffectTrailSampleProvider> sampleProvider{};
        bool isDraining{ false };
        float drainElapsed{ 0.f };
        float drainDuration{ 0.25f };
    };

private: //## Data::PreviewRuntime
    wstring _layerTag{};
    EffectInstancePool::AttachedHandle _previewPoolHandle{};
    Shared<const EffectDefinition> _currentPreviewDefinition{};
    Shared<const EffectDefinition> _currentPreviewTrailDefinition{};
    vector<PreviewTrailStrokeRuntime> _previewTrailStrokes{};
    vector<AuthoringEmitter> _currentAuthoringEmitters{};
    bool _wasTrailPreviewGateOpen{ false };
    float _lastTrailPreviewSourceRatio{ 0.f };
    bool _hasTrailPreviewSourceRatio{ false };
    Weak<IEffectTrailSampleProvider> _trailPreviewSampleProvider{};
    Shared<IEffectSourcePointSampleProvider> _ribbonPreviewSampleProvider{};
    Shared<IEffectSourcePointSampleProvider> _sourceHistoryPreviewSampleProvider{};
    Shared<IEffectSourcePointSampleProvider> _sourceHistorySpriteTrailPreviewSampleProvider{};
    Weak<TrailPreviewController> _trailPreviewController{};

private: //## Helper::PreviewRuntime
    HRESULT Restart_FromDefinition(const Shared<const EffectDefinition>& previewDefinition);
    void Update_PreviewTrailStrokes(float timeDelta);
    HRESULT Start_PreviewTrailStroke();
    void Begin_PreviewTrailStrokeDrain(PreviewTrailStrokeRuntime& runtime);
    void Drain_ActivePreviewTrailStrokes();
    void Clear_PreviewTrailStrokes();
};

NS_END
