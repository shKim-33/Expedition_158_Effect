#pragma once

#include "Editor_Window.h"
#include "EffectMaterialAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class EffectMaterialPreviewRenderer;

class EffectMaterial_View final : public Editor_Window
{
public:
    EffectMaterial_View();
    ~EffectMaterial_View() override;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;

public: //## Behavior::Selection
    void Open_PresetBrowser();
    void Open_Preset(const wstring& filePath);

public: //## Accessors::Preset
    bool Has_SelectedPreset() const;
    const EffectMaterialInstanceData& Get_SelectedPresetMaterial() const;
    string Get_SelectedMaterialFileName() const;

private: //## Data::Preset
    vector<string> _presetGuids{};
    wstring _selectedPresetPath{};
    string _selectedPresetGuid{};
    string _selectedPresetType{};
    EffectMaterialInstanceData _selectedPresetMaterial{};
    string _statusMessage{};
    Unique<EffectMaterialPreviewRenderer> _previewRenderer{};

private: //## Helper::PresetView
    void Render_PresetInfo();
    void Render_PresetPreview(bool fillAvailableHeight);
    void Render_MaterialInstanceTable(const char* tableId, const EffectMaterialInstanceData& material) const;
    void Refresh_PresetList();
    bool Load_PresetGuid(const string& guid);
    bool Load_PresetFile(const wstring& filePath);

public:
    static Shared<EffectMaterial_View> Create();
};

NS_END
