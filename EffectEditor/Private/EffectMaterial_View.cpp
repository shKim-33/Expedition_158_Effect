#include "EffectMaterial_View.h"

#include <fstream>
#include "EffectMaterialPresetReader.h"
#include "EffectMaterialPreviewRenderer.h"
#include "GameInstance.h"
#include "Helper_String.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr float kPreviewPaneWidthRatio = 0.42f;
    constexpr float kPreviewPaneMinWidth = 420.f;
    constexpr float kPreviewPaneMaxWidth = 640.f;
    constexpr float kSideBySideLayoutMinWidth = 760.f;

    const char* Get_BlendModeLabel(EffectMaterialBlendMode value)
    {
        switch (value)
        {
        case EffectMaterialBlendMode::AlphaBlend:
            return "AlphaBlend";
        case EffectMaterialBlendMode::Additive:
            return "Additive";
        case EffectMaterialBlendMode::Masked:
            return "Masked";
        case EffectMaterialBlendMode::Modulate:
            return "Modulate";
        default:
            return "AlphaBlend";
        }
    }

    const char* Get_AdditiveColorSourceLabel(EffectMaterialAdditiveColorSource value)
    {
        switch (value)
        {
        case EffectMaterialAdditiveColorSource::MainRGB:
            return "Main RGB";
        case EffectMaterialAdditiveColorSource::TintedRGB:
            return "Tinted RGB";
        case EffectMaterialAdditiveColorSource::EmissiveColor:
            return "Emissive Color";
        case EffectMaterialAdditiveColorSource::ConstantColor:
            return "Constant Color";
        default:
            return "Main RGB";
        }
    }

    const char* Get_AdditiveAmountSourceLabel(EffectMaterialAdditiveAmountSource value)
    {
        switch (value)
        {
        case EffectMaterialAdditiveAmountSource::Alpha:
            return "Alpha";
        case EffectMaterialAdditiveAmountSource::Red:
            return "Red";
        case EffectMaterialAdditiveAmountSource::Luminance:
            return "Luminance";
        case EffectMaterialAdditiveAmountSource::Mask:
            return "Mask";
        case EffectMaterialAdditiveAmountSource::One:
            return "One";
        default:
            return "Alpha";
        }
    }

    const char* Get_AdditiveCoveragePolicyLabel(EffectMaterialAdditiveCoveragePolicy value)
    {
        switch (value)
        {
        case EffectMaterialAdditiveCoveragePolicy::AmountOnly:
            return "Amount Only";
        case EffectMaterialAdditiveCoveragePolicy::CoverageAndAmount:
            return "Coverage And Amount";
        case EffectMaterialAdditiveCoveragePolicy::Independent:
            return "Independent";
        default:
            return "Amount Only";
        }
    }

    bool Is_SupportedOpacitySource(const string& opacitySource)
    {
        return
            opacitySource == "Alpha" ||
            opacitySource == "alpha" ||
            opacitySource == "Red" ||
            opacitySource == "red" ||
            opacitySource == "Luminance" ||
            opacitySource == "luminance";
    }

    bool Should_WarnInvalidSubUv(const json& root, const char* key)
    {
        const auto iter = root.find(key);
        if (iter == root.end())
            return false;

        if (!iter->is_number_unsigned())
            return true;

        return iter->get<uint32>() == 0;
    }
}

EffectMaterial_View::EffectMaterial_View()
    : Editor_Window{ L"Effect Material", ICON_FA_CIRCLE_INFO }
    , _previewRenderer{ EffectMaterialPreviewRenderer::Create() }
{
}

EffectMaterial_View::~EffectMaterial_View() = default;

void EffectMaterial_View::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if (Is_Open() && _previewRenderer != nullptr)
        _previewRenderer->Update_MaterialTime(timeDelta);
}

void EffectMaterial_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (_selectedPresetPath.empty() || _previewRenderer == nullptr)
        return;

    _previewRenderer->Render_Material(_selectedPresetMaterial);
}

void EffectMaterial_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();
    Apply_PendingFocusBeforeBegin();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        Clear_PendingFocusAfterBegin();
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        Render_PresetInfo();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void EffectMaterial_View::Open_PresetBrowser()
{
    _statusMessage.clear();
    Refresh_PresetList();

    if (_selectedPresetPath.empty() && !_presetGuids.empty())
        Load_PresetGuid(_presetGuids.front());

    Set_Open(true);
    Request_FocusOnOpen();
}

void EffectMaterial_View::Open_Preset(const wstring& filePath)
{
    _statusMessage.clear();
    Refresh_PresetList();
    Load_PresetFile(filePath);
    Set_Open(true);
    Request_FocusOnOpen();
}

bool EffectMaterial_View::Has_SelectedPreset() const
{
    return !_selectedPresetPath.empty();
}

const EffectMaterialInstanceData& EffectMaterial_View::Get_SelectedPresetMaterial() const
{
    return _selectedPresetMaterial;
}

string EffectMaterial_View::Get_SelectedMaterialFileName() const
{
    if (_selectedPresetPath.empty())
        return {};

    return String::ToString(fs::path(_selectedPresetPath).filename().wstring());
}

void EffectMaterial_View::Render_PresetInfo()
{
    if (_selectedPresetPath.empty())
    {
        if (!_statusMessage.empty())
            ImGui::TextDisabled("%s", _statusMessage.c_str());
        else
            ImGui::TextDisabled("선택된 원본 머티리얼이 없습니다.");
        return;
    }

    auto render_detail = [this]
    {
        ImGui::TextDisabled("읽기 전용 원본 머티리얼입니다. Required slot에 복사해 사용합니다.");

        if (!_statusMessage.empty())
            ImGui::TextDisabled("%s", _statusMessage.c_str());

        if (ImGui::BeginTable("EffectMaterialPresetMeta", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.72f);

            const string fileName = String::ToString(fs::path(_selectedPresetPath).filename().wstring());
            const string fullPath = String::ToString(_selectedPresetPath);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("원본 머티리얼 파일");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(fileName.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("경로");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", fullPath.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("GUID");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(_selectedPresetGuid.empty() ? "(pending)" : _selectedPresetGuid.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("Type");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(_selectedPresetType.empty() ? "EffectMaterial" : _selectedPresetType.c_str());

            ImGui::EndTable();
        }

        ImGui::Separator();
        Render_MaterialInstanceTable("EffectMaterialPresetInstance", _selectedPresetMaterial);
    };

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const bool useSideBySideLayout = contentWidth >= kSideBySideLayoutMinWidth;
    if (useSideBySideLayout)
    {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float previewPaneWidth = clamp(
            contentWidth * kPreviewPaneWidthRatio,
            kPreviewPaneMinWidth,
            kPreviewPaneMaxWidth
        );

        ImGui::BeginChild("EffectMaterialPreviewPane", ImVec2(previewPaneWidth, 0.f), false);
        {
            Render_PresetPreview(true);
        }
        ImGui::EndChild();

        ImGui::SameLine(0.f, spacing);

        ImGui::BeginChild("EffectMaterialPresetDetail", ImVec2(0.f, 0.f), false);
        {
            render_detail();
        }
        ImGui::EndChild();
    }
    else
    {
        Render_PresetPreview(false);
        ImGui::Separator();
        render_detail();
    }
}

void EffectMaterial_View::Render_PresetPreview(bool fillAvailableHeight)
{
    if (_previewRenderer == nullptr)
        return;

    _previewRenderer->Draw_Viewport(
        "EffectMaterialOriginalPreview",
        _selectedPresetMaterial,
        "",
        fillAvailableHeight
    );
}

void EffectMaterial_View::Render_MaterialInstanceTable(const char* tableId, const EffectMaterialInstanceData& material) const
{
    if (!ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        return;

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.35f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

    const auto draw_row = [](const char* label, const string& value)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(value.c_str());
    };

    const auto draw_texture_row = [](const char* label, const string& textureGuid, const string& texturePath)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);

        const string displayText = texturePath.empty() ? textureGuid : texturePath;
        ImGui::TextUnformatted(displayText.c_str());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            const string tooltip =
                "GUID: " + (textureGuid.empty() ? string{ "(none)" } : textureGuid) +
                "\nPath: " + (texturePath.empty() ? string{ "(none)" } : texturePath);
            ImGui::SetTooltip("%s", tooltip.c_str());
        }
    };

    draw_texture_row("Main Texture (Emissive)", material.mainTextureGuid, material.mainTexturePath);
    draw_texture_row("Noise Texture", material.noiseTextureGuid, material.noiseTexturePath);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Tint");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("(%.2f, %.2f, %.2f, %.2f)", material.tint.x, material.tint.y, material.tint.z, material.tint.w);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Intensity");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", material.intensity);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Opacity Power");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", material.opacityPower);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Noise Strength");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", material.noiseStrength);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Core Emissive");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", material.coreEmissive.enabled ? "Enabled" : "Disabled");

    if (material.coreEmissive.enabled)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Core Power / Intensity");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.2f / %.2f", material.coreEmissive.corePower, material.coreEmissive.coreIntensity);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Outer Power / Intensity");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.2f / %.2f", material.coreEmissive.outerPower, material.coreEmissive.outerIntensity);
    }

    draw_row("Preset Blend Mode", Get_BlendModeLabel(material.blendMode));
    if (material.blendMode == EffectMaterialBlendMode::Additive)
    {
        draw_row("Additive Color Source", Get_AdditiveColorSourceLabel(material.additive.colorSource));
        draw_row("Additive Amount Source", Get_AdditiveAmountSourceLabel(material.additive.amountSource));
        draw_row("Additive Coverage", Get_AdditiveCoveragePolicyLabel(material.additive.coveragePolicy));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Additive Intensity");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.2f", material.additive.intensityScale);
    }
    else
        draw_row("Opacity Source", material.opacitySource);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("SubUV");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%u cols x %u rows", material.subUVCols, material.subUVRows);

    ImGui::EndTable();
}

void EffectMaterial_View::Refresh_PresetList()
{
    _presetGuids.clear();

    if (GAME == nullptr)
        return;

    const vector<AssetMeta> presetAssets = GAME->Get_AssetsByType("EffectMaterial");
    for (const AssetMeta& assetMeta : presetAssets)
    {
        if (assetMeta.guid.empty() || assetMeta.fullPath.empty())
            continue;

        _presetGuids.push_back(assetMeta.guid);
    }
}

bool EffectMaterial_View::Load_PresetGuid(const string& guid)
{
    if (guid.empty())
    {
        _selectedPresetPath.clear();
        _selectedPresetGuid.clear();
        _selectedPresetType.clear();
        _selectedPresetMaterial = EffectMaterialInstanceData{};
        _statusMessage = "Material GUID is empty.";
        return false;
    }

    if (GAME == nullptr)
    {
        _selectedPresetPath.clear();
        _selectedPresetGuid.clear();
        _selectedPresetType.clear();
        _selectedPresetMaterial = EffectMaterialInstanceData{};
        _statusMessage = "Asset manager is unavailable.";
        return false;
    }

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(guid);
    if (assetMeta == nullptr || assetMeta->fullPath.empty())
    {
        _selectedPresetPath.clear();
        _selectedPresetGuid.clear();
        _selectedPresetType.clear();
        _selectedPresetMaterial = EffectMaterialInstanceData{};
        _statusMessage = "Material GUID could not be resolved.";
        LOG_ERROR("EffectMaterial material preset load failed: guid resolve failed. guid='{}'", guid);
        return false;
    }

    if (!assetMeta->type.empty() && assetMeta->type != "EffectMaterial")
    {
        _selectedPresetPath.clear();
        _selectedPresetGuid.clear();
        _selectedPresetType.clear();
        _selectedPresetMaterial = EffectMaterialInstanceData{};
        _statusMessage = "Selected GUID is not an EffectMaterial.";
        LOG_ERROR(
            "EffectMaterial material preset load failed: guid type mismatch. guid='{}', type='{}'",
            guid,
            assetMeta->type
        );
        return false;
    }

    if (!Load_PresetFile(assetMeta->fullPath))
        return false;

    _selectedPresetGuid = guid;
    _selectedPresetType = assetMeta->type.empty() ? "EffectMaterial" : assetMeta->type;
    return true;
}

bool EffectMaterial_View::Load_PresetFile(const wstring& filePath)
{
    _statusMessage.clear();
    _selectedPresetPath.clear();
    _selectedPresetGuid.clear();
    _selectedPresetType.clear();
    _selectedPresetMaterial = EffectMaterialInstanceData{};

    if (!fs::exists(filePath))
    {
        _statusMessage = "Material file does not exist.";
        LOG_ERROR("EffectMaterial material preset load failed: file does not exist. path='{}'", String::ToString(filePath));
        return false;
    }

    ifstream file(filePath);
    if (!file.is_open())
    {
        _statusMessage = "Failed to open material file.";
        LOG_ERROR("EffectMaterial material preset load failed: open failed. path='{}'", String::ToString(filePath));
        return false;
    }

    json root{};
    try
    {
        file >> root;
    }
    catch (...)
    {
        _statusMessage = "Failed to parse material json.";
        LOG_ERROR("EffectMaterial material preset load failed: json parse failed. path='{}'", String::ToString(filePath));
        return false;
    }

    EffectMaterialInstanceData material{};
    if (!EffectMaterialPresetReader::Read(filePath, material))
    {
        _statusMessage = "Failed to read material preset.";
        LOG_ERROR("EffectMaterial material preset load failed: material read failed. path='{}'", String::ToString(filePath));
        return false;
    }
    const bool warnSubUVRows = Should_WarnInvalidSubUv(root, "subUVRows");
    const bool warnSubUVCols = Should_WarnInvalidSubUv(root, "subUVCols");

    const string presetPath = String::ToString(filePath);
    if (warnSubUVRows)
        LOG_WARN("EffectMaterial material preset corrected invalid subUVRows to 1. path='{}'", presetPath);
    if (warnSubUVCols)
        LOG_WARN("EffectMaterial material preset corrected invalid subUVCols to 1. path='{}'", presetPath);

    if (!Is_SupportedOpacitySource(material.opacitySource))
    {
        _statusMessage = "Unsupported opacitySource in material preset.";
        LOG_ERROR(
            "EffectMaterial material preset load failed: unsupported opacitySource='{}'. path='{}'",
            material.opacitySource,
            presetPath
        );
        return false;
    }

    if (material.mainTextureGuid.empty() && material.mainTexturePath.empty())
    {
        _statusMessage = "Main texture reference is empty.";
        LOG_ERROR(
            "EffectMaterial material preset load failed: main texture reference is empty. path='{}'",
            presetPath
        );
        return false;
    }

    _selectedPresetPath = filePath;
    _selectedPresetMaterial = material;
    _selectedPresetGuid = GAME ? GAME->Find_AssetGUID(filePath) : string{};
    _selectedPresetType = "EffectMaterial";

    if (!_selectedPresetGuid.empty())
    {
        if (const AssetMeta* assetMeta = GAME->Find_AssetByGUID(_selectedPresetGuid))
            _selectedPresetType = assetMeta->type;
    }

    return true;
}

Shared<EffectMaterial_View> EffectMaterial_View::Create()
{
    return make_shared<EffectMaterial_View>();
}

NS_END
