#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(EffectEditor)

class EffectEditorApp final : public Base
{
public:
    EffectEditorApp();
    ~EffectEditorApp() override;

public:
    HRESULT Initialize();
    void Priority_Update(float timeDelta);
    void Update(float timeDelta);
    void Late_Update(float timeDelta);
    HRESULT Render();

private: //## Types::RenderPerf
#ifdef _DEBUG
    struct PerfCounter
    {
        size_t sampleCount{};
        double accumulatedMs{};
        double maxMs{};
    };

    struct EffectEditorRenderPerfWindow
    {
        uint32 observedFrameCount{};
        PerfCounter totalRender{};
        PerfCounter gameRender{};
        PerfCounter capture{};
        PerfCounter bindBackBuffer{};
        PerfCounter editorRender{};
        PerfCounter editorWindows{};
        PerfCounter notification{};
        PerfCounter imgui{};
        PerfCounter present{};
    };
#endif

private: //## Static::RenderPerformance
    inline static const Vec4 kDefaultClearColor{ 0.18f, 0.18f, 0.18f, 1.f };
#ifdef _DEBUG
    static constexpr float kEffectEditorRenderPerfSampleIntervalSec{ 0.25f };
    static constexpr float kEffectEditorRenderPerfWindowSec{ 5.f };
#endif

private: //## Data::Core
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};
    fs::path _assetRootPath{};
    fs::path _effectsAssetScanRoot{};
    mutex _assetDeltaScanMutex{};
    optional<AssetDeltaScanResult> _pendingAssetDeltaScanResult{};
    bool _assetDeltaScanQueued{ false };
    bool _assetDeltaScanCommitted{ false };
#ifdef _DEBUG
    EffectEditorRenderPerfWindow _renderPerfWindow{};
    chrono::steady_clock::time_point _renderPerfWindowStartTime{};
    chrono::steady_clock::time_point _renderPerfLastSampleTime{};
    bool _hasRenderPerfWindowStartTime{ false };
    bool _hasRenderPerfLastSampleTime{ false };
#endif

private: //## Helper::Setup
    HRESULT Ready_Fonts();
    HRESULT Ready_Prototype_For_Static_Level();
    HRESULT Ready_PreviewLevel();

    wstring Get_ClientResourceRoot() const;
    wstring Get_ClientResourcePath(const wchar_t* relativePath) const;
    void Queue_EffectsAssetDeltaScan();
    void Commit_PendingAssetDeltaScan();

#ifdef _DEBUG
private: //## Helper::RenderPerf
    bool Should_SampleEffectEditorRenderPerf();
    void Reset_EffectEditorRenderPerfWindowState(chrono::steady_clock::time_point now);
    void Accumulate_EffectEditorRenderPerf(
        double totalRenderMs,
        double gameRenderMs,
        double captureMs,
        double bindBackBufferMs,
        double editorRenderMs,
        double editorWindowsMs,
        double notificationMs,
        double imguiMs,
        double presentMs);
    void Try_FlushEffectEditorRenderPerfLog();

    static void Accumulate_PassPerf(PerfCounter& counter, double passMs);
    static double Safe_Divide(double numerator, double denominator);
#endif

public:
    static Unique<EffectEditorApp> Create();
    void Free() override;
};

NS_END
