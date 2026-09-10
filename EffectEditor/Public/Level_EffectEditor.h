#pragma once

#include "EffectAuthoring_Types.h"
#include "Level.h"

NS_BEGIN(EffectEditor)

class EffectEditorPreviewRuntime;
class TrailPreviewController;
class TrailPreviewSampleRouter;

class Level_EffectEditor final : public Level
{
public:
    Level_EffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~Level_EffectEditor() override;

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Restart_PreviewFromAuthoring();
    HRESULT Reset_PreviewRuntime();
    bool Is_PreviewFinished() const;
    void Apply_PreviewEmitterTransformOverrides(const vector<AuthoringEmitter>& emitters);
    bool Try_GetPreviewEmitterWorldMatrix(uint32 emitterId, Matrix& outWorldMatrix) const;
    HRESULT Ensure_TrailPreviewFixture();
    void Get_TrailPreviewAnimationNames(vector<string>& outNames) const;

private: //## Static::Preview
    static constexpr auto kPreviewLayerTag{ L"Layer_Preview" };

private: //## Data::PreviewRuntime
    Unique<EffectEditorPreviewRuntime> _previewRuntime{};
    bool _trailPreviewFixtureReady{ false };
    Weak<TrailPreviewController> _trailPreviewController{};
    Shared<TrailPreviewSampleRouter> _trailPreviewSampleRouter{};

private: //## Helper::Setup
    HRESULT Ready_Lights();
    HRESULT Ready_Layer_Camera(const wstring& layerTag);
    HRESULT Ready_Layer_Grid(const wstring& layerTag);
    HRESULT Ready_Layer_Sky(const wstring& layerTag);
    HRESULT Ready_Layer_Preview(const wstring& layerTag);

public:
    static Shared<Level_EffectEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
