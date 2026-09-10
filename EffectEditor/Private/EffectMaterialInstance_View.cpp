#include "EffectMaterialInstance_View.h"

#include "DetailPropertyContext.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_String.h"
#include "Notification_Manager.h"
#include "Texture.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kEffectTexturePayloadType = "CONTENT_BROWSER_EFFECT_TEXTURE";
    constexpr auto kTextureDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Textures";
    constexpr float kPreviewPaneWidthRatio = 0.42f;
    constexpr float kPreviewPaneMinWidth = 420.f;
    constexpr float kPreviewPaneMaxWidth = 640.f;

    string textureAssignStatus{};

    string ToLowerCopy(string value)
    {
        ranges::transform(value, value.begin(), ::tolower);
        return value;
    }

    bool Has_DuplicateEmitterName(const vector<AuthoringEmitter>& emitters, const AuthoringEmitter& target)
    {
        if (target.name.empty())
            return false;

        uint32 matchCount = 0;
        for (const AuthoringEmitter& emitter : emitters)
        {
            if (emitter.name == target.name)
                ++matchCount;
        }

        return matchCount > 1;
    }

    string Build_EmitterPreviewLabel(const vector<AuthoringEmitter>& emitters, const AuthoringEmitter& emitter, const char* previewKind)
    {
        string label = emitter.name.empty() ? "Emitter" : emitter.name;
        if (emitter.name.empty() || Has_DuplicateEmitterName(emitters, emitter))
            label += " #" + std::to_string(emitter.id);

        label += " - ";
        label += previewKind;
        return label;
    }

    wstring ToLowerCopy(wstring value)
    {
        ranges::transform(
            value,
            value.begin(),
            [](wchar_t character)
            {
                return static_cast<wchar_t>(towlower(character));
            }
        );
        return value;
    }

    bool Is_SupportedTextureFile(const fs::path& filePath)
    {
        const string extension = ToLowerCopy(filePath.extension().string());
        return extension == ".png" || extension == ".dds";
    }

    bool Is_PngTextureFile(const fs::path& filePath)
    {
        return ToLowerCopy(filePath.extension().string()) == ".png";
    }

    fs::path Normalize_Path(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {
        }

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {
        }

        return filePath.lexically_normal();
    }

    fs::path Resolve_EditorPath(const wchar_t* path)
    {
        return Normalize_Path(fs::path(path));
    }

    string Make_ResourceRelativePath(const fs::path& filePath)
    {
        if (GAME != nullptr)
        {
            const fs::path assetRoot = Normalize_Path(GAME->Get_AssetRoot());
            if (!assetRoot.empty())
            {
                const fs::path normalizedPath = Normalize_Path(filePath);
                const fs::path relativePath = normalizedPath.lexically_relative(assetRoot);
                if (!relativePath.empty() && !relativePath.native().starts_with(L".."))
                {
                    string result = String::ToString(relativePath.wstring());
                    ranges::replace(result, '\\', '/');
                    return result;
                }
            }
        }

        string result = String::ToString(Normalize_Path(filePath).wstring());
        ranges::replace(result, '\\', '/');
        return result;
    }

    wstring Resolve_InstanceTexturePathByGuid(const string& textureGuid)
    {
        if (GAME == nullptr || textureGuid.empty())
            return {};

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture")
            return {};

        const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
        return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
    }

    wstring Resolve_InstanceTexturePathByFallbackPath(const string& texturePath)
    {
        if (!texturePath.empty())
        {
            fs::path candidatePath = String::ToWString(texturePath);
            if (candidatePath.is_relative() && GAME != nullptr)
                candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

            candidatePath = Normalize_Path(candidatePath);
            if (fs::exists(candidatePath))
                return candidatePath.wstring();
        }

        return {};
    }

    wstring Resolve_InstanceTexturePath(const string& textureGuid, const string& texturePath)
    {
        wstring resolvedPath = Resolve_InstanceTexturePathByGuid(textureGuid);
        if (!resolvedPath.empty())
            return resolvedPath;

        return Resolve_InstanceTexturePathByFallbackPath(texturePath);
    }

    fs::path Resolve_PreferredTexturePath(const fs::path& filePath, bool& outSwitchedToDds)
    {
        outSwitchedToDds = false;

        fs::path targetPath = Normalize_Path(filePath);
        if (!Is_PngTextureFile(targetPath))
            return targetPath;

        fs::path ddsPath = targetPath;
        ddsPath.replace_extension(".dds");

        if (!fs::exists(ddsPath))
            return targetPath;

        outSwitchedToDds = true;
        return Normalize_Path(ddsPath);
    }

    void Notify_TextureAssignment(NotifyType type, const string& message)
    {
        if (EDITOR == nullptr || EDITOR->Get_Notification() == nullptr)
            return;

        EDITOR->Get_Notification()->Add_Notification_With_Type(type, "{}", message);
    }

    bool Try_AssignTextureFromFile(const fs::path& filePath, string& textureGuid, string& texturePath)
    {
        if (!Is_SupportedTextureFile(filePath))
        {
            textureAssignStatus = "Unsupported texture suffix. Use .png or .dds.";
            return false;
        }

        if (!fs::exists(filePath))
        {
            textureAssignStatus = "Texture file not found.";
            return false;
        }

        bool switchedToDds = false;
        const fs::path targetPath = Resolve_PreferredTexturePath(filePath, switchedToDds);

        texturePath = Make_ResourceRelativePath(targetPath);
        textureGuid = GAME != nullptr ? GAME->Ensure_AssetGUID(targetPath.wstring(), "Texture") : string{};

        if (switchedToDds)
        {
            const string sourcePath = Make_ResourceRelativePath(filePath);
            textureAssignStatus = "Texture switched PNG -> DDS: " + sourcePath + " -> " + texturePath;
            Notify_TextureAssignment(NotifyType::Info, textureAssignStatus);
        }
        else
            textureAssignStatus = "Texture assigned: " + texturePath;

        if (textureGuid.empty())
            Notify_TextureAssignment(NotifyType::Warning, textureAssignStatus + " (GUID cache unavailable)");

        return true;
    }

    bool Try_PickTextureFileDialog(wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(L"Select Effect Texture");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Resolve_EditorPath(kTextureDialogDefaultFolder);
        if (!defaultFolder.empty())
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"Effect Textures", L"*.png;*.dds" },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    Shared<Texture> Resolve_TextureThumbnail(
        map<string, Shared<Texture>>& textureCache,
        const string& textureGuid,
        const string& texturePath)
    {
        if (EDITOR == nullptr)
            return nullptr;

        const wstring resolvedPath = Resolve_InstanceTexturePath(textureGuid, texturePath);
        if (resolvedPath.empty())
            return nullptr;

        const string cacheKey = String::ToString(resolvedPath);
        const auto iter = textureCache.find(cacheKey);
        if (iter != textureCache.end())
            return iter->second;

        Shared<Texture> texture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), resolvedPath.c_str(), 1);
        textureCache[cacheKey] = texture;
        return texture;
    }

    ShaderResourceView* Get_TextureThumbnailSRV(
        map<string, Shared<Texture>>& textureCache,
        const string& textureGuid,
        const string& texturePath)
    {
        const Shared<Texture> texture = Resolve_TextureThumbnail(textureCache, textureGuid, texturePath);
        if (texture == nullptr || texture->Get_SRVs().empty())
            return nullptr;

        return texture->Get_SRVs().front().Get();
    }

    void Draw_TextureThumbnail(
        map<string, Shared<Texture>>& textureCache,
        const string& textureGuid,
        const string& texturePath,
        const ImVec2& size)
    {
        ShaderResourceView* srv = Get_TextureThumbnailSRV(textureCache, textureGuid, texturePath);
        if (srv != nullptr)
        {
            const ImTextureID texture = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
            ImGui::Image(texture, size);
            return;
        }

        const ImVec2 minPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##TextureThumbnailFallback", size);
        const ImVec2 maxPos{ minPos.x + size.x, minPos.y + size.y };
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(minPos, maxPos, IM_COL32(18, 20, 24, 255), 4.f);
        drawList->AddRect(minPos, maxPos, IM_COL32(85, 96, 116, 255), 4.f);
        drawList->AddText(
            ImVec2(minPos.x + 7.f, minPos.y + size.y * 0.5f - 7.f),
            IM_COL32(150, 160, 176, 255),
            texturePath.empty() && textureGuid.empty() ? "NONE" : "TEX"
        );
    }

    string Format_TextureReference(const string& textureGuid, const string& texturePath)
    {
        if (!texturePath.empty())
            return texturePath;

        if (!textureGuid.empty())
            return textureGuid;

        return "(none)";
    }

    string Make_TextureReferenceTooltip(const string& textureGuid, const string& texturePath)
    {
        string state = "Empty";
        if (!textureGuid.empty())
        {
            const bool guidResolved = !Resolve_InstanceTexturePathByGuid(textureGuid).empty();
            if (guidResolved && !texturePath.empty() && Resolve_InstanceTexturePathByFallbackPath(texturePath).empty())
                state = "GUID reference (path fallback missing)";
            else
                state = guidResolved ? "GUID reference" : "GUID missing";
        }
        else if (!texturePath.empty())
        {
            state = Resolve_InstanceTexturePathByFallbackPath(texturePath).empty()
                    ? "Path fallback missing"
                    : "Path fallback";
        }

        return
            "State: " + state +
            "\n" +
            "GUID: " + (textureGuid.empty() ? string{ "(none)" } : textureGuid) +
            "\nPath: " + (texturePath.empty() ? string{ "(none)" } : texturePath);
    }

    bool Is_WarningTextureReference(const string& textureGuid, const string& texturePath)
    {
        if (!textureGuid.empty())
            return Resolve_InstanceTexturePathByGuid(textureGuid).empty();

        if (!texturePath.empty())
            return Resolve_InstanceTexturePathByFallbackPath(texturePath).empty();

        return false;
    }

    bool Draw_TextureSlotBody(string& textureGuid, string& texturePath, const char* sourceLockTooltip = nullptr)
    {
        const bool sourceLocked = sourceLockTooltip != nullptr && sourceLockTooltip[0] != '\0';
        constexpr float browseButtonWidth{ 28.f };
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float slotWidth = max(96.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        const ImVec2 slotSize{ slotWidth, 44.f };

        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isTextureDragging =
            !sourceLocked &&
            activePayload != nullptr &&
            activePayload->IsDataType(kEffectTexturePayloadType);

        if (sourceLocked)
            ImGui::BeginDisabled();

        ImGui::InvisibleButton("##TextureDropSlot", slotSize);
        bool changed = false;
        if (!sourceLocked && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                kEffectTexturePayloadType,
                ImGuiDragDropFlags_AcceptBeforeDelivery
            ))
            {
                if (payload->IsDelivery())
                    changed = Try_AssignTextureFromFile(static_cast<const wchar_t*>(payload->Data), textureGuid, texturePath);
            }
            ImGui::EndDragDropTarget();
        }

        const ImVec2 slotMin = ImGui::GetItemRectMin();
        const ImVec2 slotMax = ImGui::GetItemRectMax();
        const bool isHovered = ImGui::IsItemHovered(sourceLocked ? ImGuiHoveredFlags_AllowWhenDisabled : ImGuiHoveredFlags_None);
        const bool isWarningReference = Is_WarningTextureReference(textureGuid, texturePath);
        const ImU32 fillColor = sourceLocked
                                ? IM_COL32(24, 25, 29, 255)
                                : isWarningReference
                                  ? (isHovered || isTextureDragging ? IM_COL32(58, 32, 34, 255) : IM_COL32(34, 22, 24, 255))
                                  : isHovered || isTextureDragging ? IM_COL32(34, 42, 58, 255) : IM_COL32(22, 25, 32, 255);
        const ImU32 borderColor = sourceLocked
                                  ? IM_COL32(58, 62, 72, 255)
                                  : isWarningReference
                                    ? IM_COL32(214, 86, 96, 255)
                                    : isHovered || isTextureDragging ? IM_COL32(116, 156, 235, 255) : IM_COL32(78, 88, 108, 255);
        const ImU32 textColor = sourceLocked
                                ? IM_COL32(142, 148, 160, 255)
                                : isWarningReference
                                  ? IM_COL32(255, 190, 196, 255)
                                  : IM_COL32(220, 226, 238, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 4.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 4.f, 0, isTextureDragging ? 2.f : 1.f);

        const string displayText = Format_TextureReference(textureGuid, texturePath);
        drawList->PushClipRect(ImVec2(slotMin.x + 8.f, slotMin.y), ImVec2(slotMax.x - 8.f, slotMax.y), true);
        drawList->AddText(
            ImVec2(slotMin.x + 10.f, slotMin.y + slotSize.y * 0.5f - ImGui::GetTextLineHeight() * 0.5f),
            textColor,
            displayText.c_str()
        );
        drawList->PopClipRect();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | (sourceLocked ? ImGuiHoveredFlags_AllowWhenDisabled : ImGuiHoveredFlags_None)))
        {
            const string tooltip = sourceLocked
                                   ? string{ sourceLockTooltip } + "\n\n" + Make_TextureReferenceTooltip(textureGuid, texturePath)
                                   : "Content Browser의 .png / .dds를 드롭하면 현재 instance texture reference로 지정합니다.\n\n" +
                                     Make_TextureReferenceTooltip(textureGuid, texturePath);
            ImGui::SetTooltip("%s", tooltip.c_str());
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickTexture", ImVec2{ browseButtonWidth, slotSize.y }))
        {
            wstring pickedFilePath{};
            if (Try_PickTextureFileDialog(pickedFilePath))
                changed = Try_AssignTextureFromFile(pickedFilePath, textureGuid, texturePath);
        }
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | (sourceLocked ? ImGuiHoveredFlags_AllowWhenDisabled : ImGuiHoveredFlags_None)))
            ImGui::SetTooltip("%s", sourceLocked ? sourceLockTooltip : "텍스처 파일 선택");

        if (sourceLocked)
            ImGui::EndDisabled();

        return changed;
    }

    bool Draw_TextureProperty(
        map<string, Shared<Texture>>& textureCache,
        DetailPropertyContext& detailContext,
        const char* label,
        string& textureGuid,
        string& texturePath,
        const string& defaultGuid,
        const string& defaultPath,
        const char* tooltip = nullptr,
        const char* sourceLockTooltip = nullptr)
    {
        const bool sourceLocked = sourceLockTooltip != nullptr && sourceLockTooltip[0] != '\0';
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);

        constexpr ImVec2 thumbnailSize{ 44.f, 44.f };
        Draw_TextureThumbnail(textureCache, textureGuid, texturePath, thumbnailSize);
        ImGui::SameLine();
        bool changed = Draw_TextureSlotBody(textureGuid, texturePath, sourceLockTooltip);

        const bool resetClicked = detailContext.Draw_ResetButton(!sourceLocked && (textureGuid != defaultGuid || texturePath != defaultPath));
        if (resetClicked)
        {
            textureGuid = defaultGuid;
            texturePath = defaultPath;
            textureAssignStatus = texturePath.empty() ? "Texture reset: (none)" : "Texture reset: " + texturePath;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool Equals_IgnoreCase(const string& lhs, const char* rhs)
    {
        return ToLowerCopy(lhs) == ToLowerCopy(rhs);
    }

    bool Draw_StringComboProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        string& value,
        const string& defaultValue,
        const char* const* options,
        size_t optionCount,
        const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);

        const char* preview = value.empty() ? "(none)" : value.c_str();
        for (size_t i = 0; i < optionCount; ++i)
        {
            if (Equals_IgnoreCase(value, options[i]))
            {
                preview = options[i];
                break;
            }
        }

        bool changed = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", preview))
        {
            for (size_t i = 0; i < optionCount; ++i)
            {
                const bool selected = Equals_IgnoreCase(value, options[i]);
                if (ImGui::Selectable(options[i], selected))
                {
                    value = options[i];
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

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

    bool Draw_BlendModeProperty(
        DetailPropertyContext& detailContext,
        EffectMaterialBlendMode& value,
        EffectMaterialBlendMode defaultValue,
        const char* tooltip = nullptr)
    {
        static constexpr EffectMaterialBlendMode kModes[]{
            EffectMaterialBlendMode::AlphaBlend,
            EffectMaterialBlendMode::Additive,
            EffectMaterialBlendMode::Masked,
            EffectMaterialBlendMode::Modulate
        };

        ImGui::PushID("Blend Mode");
        detailContext.Draw_PropertyLabel("Blend Mode", tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", Get_BlendModeLabel(value)))
        {
            for (const EffectMaterialBlendMode mode : kModes)
            {
                const bool selected = value == mode;
                const bool selectable =
                    mode == EffectMaterialBlendMode::AlphaBlend ||
                    mode == EffectMaterialBlendMode::Additive ||
                    mode == EffectMaterialBlendMode::Masked;

                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_BlendModeLabel(mode), selected) && selectable)
                {
                    value = mode;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
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

    template <typename EnumType>
    bool Draw_EnumComboProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EnumType& value,
        EnumType defaultValue,
        const EnumType* options,
        size_t optionCount,
        const char* (*getLabel)(EnumType),
        const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", getLabel(value)))
        {
            for (size_t i = 0; i < optionCount; ++i)
            {
                const EnumType option = options[i];
                const bool selected = value == option;
                if (ImGui::Selectable(getLabel(option), selected))
                {
                    value = option;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool Draw_AdditivePresetProperty(DetailPropertyContext& detailContext, EffectMaterialAdditiveContributionData& additive)
    {
        struct AdditivePreset
        {
            const char* label{};
            EffectMaterialAdditiveContributionData data{};
        };

        static const AdditivePreset kPresets[]{
            {
                "RGB Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::MainRGB,
                    .amountSource = EffectMaterialAdditiveAmountSource::Alpha
                }
            },
            {
                "Tinted RGB Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::TintedRGB,
                    .amountSource = EffectMaterialAdditiveAmountSource::Alpha
                }
            },
            {
                "Emissive Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::EmissiveColor,
                    .amountSource = EffectMaterialAdditiveAmountSource::Alpha
                }
            },
            {
                "Channel Packed Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::ConstantColor,
                    .amountSource = EffectMaterialAdditiveAmountSource::Red
                }
            },
            {
                "Unmasked Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::MainRGB,
                    .amountSource = EffectMaterialAdditiveAmountSource::One
                }
            },
        };

        ImGui::PushID("Additive Preset");
        detailContext.Draw_PropertyLabel("Preset");
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;
        if (ImGui::BeginCombo("##Value", "Apply preset..."))
        {
            for (const AdditivePreset& preset : kPresets)
            {
                if (ImGui::Selectable(preset.label))
                {
                    additive = preset.data;
                    changed = true;
                }
            }

            ImGui::EndCombo();
        }

        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
        return changed;
    }

    bool Draw_AdditiveContributionPanel(
        DetailPropertyContext& detailContext,
        EffectMaterialAdditiveContributionData& additive,
        const EffectMaterialAdditiveContributionData& defaultAdditive)
    {
        bool changed = false;
        if (!ImGui::TreeNodeEx("Additive Contribution", ImGuiTreeNodeFlags_DefaultOpen))
            return changed;

        if (detailContext.Begin_PropertyTable("EffectMaterialInstanceAdditiveContribution"))
        {
            static constexpr EffectMaterialAdditiveColorSource kColorSources[]{
                EffectMaterialAdditiveColorSource::MainRGB,
                EffectMaterialAdditiveColorSource::TintedRGB,
                EffectMaterialAdditiveColorSource::EmissiveColor,
                EffectMaterialAdditiveColorSource::ConstantColor
            };
            static constexpr EffectMaterialAdditiveAmountSource kAmountSources[]{
                EffectMaterialAdditiveAmountSource::Alpha,
                EffectMaterialAdditiveAmountSource::Red,
                EffectMaterialAdditiveAmountSource::Luminance,
                EffectMaterialAdditiveAmountSource::Mask,
                EffectMaterialAdditiveAmountSource::One
            };
            static constexpr EffectMaterialAdditiveCoveragePolicy kCoveragePolicies[]{
                EffectMaterialAdditiveCoveragePolicy::AmountOnly,
                EffectMaterialAdditiveCoveragePolicy::CoverageAndAmount,
                EffectMaterialAdditiveCoveragePolicy::Independent
            };

            changed |= Draw_AdditivePresetProperty(detailContext, additive);
            changed |= Draw_EnumComboProperty(
                detailContext,
                "Color Source",
                additive.colorSource,
                defaultAdditive.colorSource,
                kColorSources,
                std::size(kColorSources),
                Get_AdditiveColorSourceLabel
            );
            changed |= Draw_EnumComboProperty(
                detailContext,
                "Amount Source",
                additive.amountSource,
                defaultAdditive.amountSource,
                kAmountSources,
                std::size(kAmountSources),
                Get_AdditiveAmountSourceLabel
            );
            changed |= Draw_EnumComboProperty(
                detailContext,
                "Coverage Policy",
                additive.coveragePolicy,
                defaultAdditive.coveragePolicy,
                kCoveragePolicies,
                std::size(kCoveragePolicies),
                Get_AdditiveCoveragePolicyLabel
            );
            changed |= detailContext.Draw_ColorProperty("Emissive Color", additive.emissiveColor, defaultAdditive.emissiveColor);
            changed |= detailContext.Draw_ColorProperty("Constant Color", additive.constantColor, defaultAdditive.constantColor);
            changed |= detailContext.Draw_FloatProperty("Intensity Scale", additive.intensityScale, defaultAdditive.intensityScale);
            changed |= detailContext.Draw_BoolProperty("Black Neutral", additive.blackNeutral, defaultAdditive.blackNeutral);
            detailContext.End_PropertyTable();
        }

        ImGui::TreePop();
        return changed;
    }

    const char* Get_UVTilingModeLabel(EffectTextureUVTilingMode value)
    {
        switch (value)
        {
        case EffectTextureUVTilingMode::Wrap:
            return "Wrap";
        case EffectTextureUVTilingMode::Stretch:
            return "Stretch";
        case EffectTextureUVTilingMode::Clamp:
            return "Clamp";
        case EffectTextureUVTilingMode::Raw:
            return "Raw";
        case EffectTextureUVTilingMode::Mirror:
            return "Mirror";
        default:
            return "Wrap";
        }
    }

    bool Draw_UVTilingModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EffectTextureUVTilingMode& value,
        EffectTextureUVTilingMode defaultValue,
        const char* tooltip = nullptr)
    {
        static constexpr EffectTextureUVTilingMode kModes[]{
            EffectTextureUVTilingMode::Wrap,
            EffectTextureUVTilingMode::Stretch,
            EffectTextureUVTilingMode::Clamp,
            EffectTextureUVTilingMode::Mirror,
            EffectTextureUVTilingMode::Raw
        };

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", Get_UVTilingModeLabel(value)))
        {
            for (const EffectTextureUVTilingMode mode : kModes)
            {
                const bool selected = value == mode;
                if (ImGui::Selectable(Get_UVTilingModeLabel(mode), selected))
                {
                    value = mode;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    EffectTextureUVTilingMode Resolve_CompatibleUVTilingModeForView(const EffectMaterialUVAxisPolicy& policy)
    {
        return policy.uPolicy == policy.vPolicy ? policy.uPolicy : EffectTextureUVTilingMode::Wrap;
    }

    bool Draw_UVAxisPolicyProperties(
        DetailPropertyContext& detailContext,
        const char* uLabel,
        const char* vLabel,
        EffectMaterialUVAxisPolicy& policy,
        EffectMaterialUVAxisPolicy defaultPolicy,
        EffectTextureUVTilingMode& compatibleMode,
        const char* uTooltip = nullptr,
        const char* vTooltip = nullptr)
    {
        bool changed = false;
        changed |= Draw_UVTilingModeProperty(detailContext, uLabel, policy.uPolicy, defaultPolicy.uPolicy, uTooltip);
        changed |= Draw_UVTilingModeProperty(detailContext, vLabel, policy.vPolicy, defaultPolicy.vPolicy, vTooltip);

        if (changed)
            compatibleMode = Resolve_CompatibleUVTilingModeForView(policy);

        return changed;
    }

    const char* Get_UVRotationLabel(EffectMaterialUVRotation value)
    {
        switch (value)
        {
        case EffectMaterialUVRotation::None:
            return "None";
        case EffectMaterialUVRotation::Rotate90CW:
            return "90 CW";
        case EffectMaterialUVRotation::Rotate180:
            return "180";
        case EffectMaterialUVRotation::Rotate90CCW:
            return "90 CCW";
        default:
            return "None";
        }
    }

    bool Draw_UVRotationProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EffectMaterialUVRotation& value,
        EffectMaterialUVRotation defaultValue,
        const char* tooltip = nullptr)
    {
        static constexpr EffectMaterialUVRotation kRotations[]{
            EffectMaterialUVRotation::None,
            EffectMaterialUVRotation::Rotate90CW,
            EffectMaterialUVRotation::Rotate180,
            EffectMaterialUVRotation::Rotate90CCW
        };

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", Get_UVRotationLabel(value)))
        {
            for (const EffectMaterialUVRotation rotation : kRotations)
            {
                const bool selected = value == rotation;
                if (ImGui::Selectable(Get_UVRotationLabel(rotation), selected))
                {
                    value = rotation;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    const char* Get_MaterialFamilyLabel(EffectMaterialFamily value)
    {
        switch (value)
        {
        case EffectMaterialFamily::Default:
            return "Default";
        case EffectMaterialFamily::SpriteDistortion:
            return "SpriteDistortion";
        case EffectMaterialFamily::MeshGlass:
            return "MeshGlass";
        default:
            return "Unknown";
        }
    }

    bool Draw_MaterialFamilyProperty(
        DetailPropertyContext& detailContext,
        EffectMaterialFamily& value,
        EffectMaterialFamily defaultValue)
    {
        static constexpr EffectMaterialFamily kFamilies[]{
            EffectMaterialFamily::Default,
            EffectMaterialFamily::SpriteDistortion,
            EffectMaterialFamily::MeshGlass
        };

        return Draw_EnumComboProperty(
            detailContext,
            "Family",
            value,
            defaultValue,
            kFamilies,
            size(kFamilies),
            Get_MaterialFamilyLabel,
            "material shader/render family입니다. MeshGlass는 Mesh emitter 전용 fake glass surface로 소비됩니다."
        );
    }

    const char* Get_DistortionShapeModeLabel(EffectDistortionShapeMode value)
    {
        switch (value)
        {
        case EffectDistortionShapeMode::None:
            return "None";
        case EffectDistortionShapeMode::Radial:
            return "Radial";
        case EffectDistortionShapeMode::Ring:
            return "Ring";
        case EffectDistortionShapeMode::ShockwaveRing:
            return "ShockwaveRing";
        case EffectDistortionShapeMode::AirSheath:
            return "AirSheath";
        default:
            return "None";
        }
    }

    const char* Get_AirSheathMapInterpretationLabel(EffectAirSheathMapInterpretation value)
    {
        switch (value)
        {
        case EffectAirSheathMapInterpretation::VectorField: return "Vector Field";
        case EffectAirSheathMapInterpretation::HeightGradient: return "Height Gradient";
        case EffectAirSheathMapInterpretation::ScalarModulation: return "Scalar Modulation";
        default: return "Vector Field";
        }
    }

    const char* Get_AirSheathMapScalarSourceLabel(EffectAirSheathMapScalarSource value)
    {
        switch (value)
        {
        case EffectAirSheathMapScalarSource::Red: return "R";
        case EffectAirSheathMapScalarSource::Green: return "G";
        case EffectAirSheathMapScalarSource::Blue: return "B";
        case EffectAirSheathMapScalarSource::Alpha: return "A";
        case EffectAirSheathMapScalarSource::Luminance: return "Luminance";
        default: return "R";
        }
    }

    const char* Get_AirSheathMapVectorSpaceLabel(EffectAirSheathMapVectorSpace value)
    {
        switch (value)
        {
        case EffectAirSheathMapVectorSpace::Screen: return "Screen";
        case EffectAirSheathMapVectorSpace::TrailLocal: return "Trail Local";
        case EffectAirSheathMapVectorSpace::BaseField: return "Base Field";
        default: return "Screen";
        }
    }

    const char* Get_AirSheathMapCompositionLabel(EffectAirSheathMapComposition value)
    {
        switch (value)
        {
        case EffectAirSheathMapComposition::Replace: return "Replace";
        case EffectAirSheathMapComposition::Add: return "Add";
        case EffectAirSheathMapComposition::Modulate: return "Modulate";
        default: return "Replace";
        }
    }

    bool Draw_DistortionShapeModeProperty(
        DetailPropertyContext& detailContext,
        EffectDistortionShapeMode& value,
        EffectDistortionShapeMode defaultValue,
        EffectMaterialDistortionShapeControlMode controlMode)
    {
        static constexpr EffectDistortionShapeMode kSpriteModes[]{
            EffectDistortionShapeMode::None,
            EffectDistortionShapeMode::Radial,
            EffectDistortionShapeMode::Ring,
            EffectDistortionShapeMode::ShockwaveRing
        };
        static constexpr EffectDistortionShapeMode kTrailModes[]{
            EffectDistortionShapeMode::None,
            EffectDistortionShapeMode::AirSheath
        };

        if (controlMode == EffectMaterialDistortionShapeControlMode::TrailEditable)
        {
            return Draw_EnumComboProperty(
                detailContext,
                "Shape Mode",
                value,
                defaultValue,
                kTrailModes,
                size(kTrailModes),
                Get_DistortionShapeModeLabel,
                "Trail distortion shape입니다. None은 Flow-only, AirSheath는 폭 방향의 분석적 공기막 굴절입니다."
            );
        }

        return Draw_EnumComboProperty(
            detailContext,
            "Shape Mode",
            value,
            defaultValue,
            kSpriteModes,
            size(kSpriteModes),
            Get_DistortionShapeModeLabel,
            "굴절이 적용될 화면상의 기본 shape mask입니다. None이면 texture/flow 입력 전체를 사용합니다."
        );
    }

    void Draw_ReadOnlyTextProperty(DetailPropertyContext& detailContext, const char* label, const char* value, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::TextDisabled("%s", value);
        ImGui::PopID();
    }

    void Draw_ReadOnlyFloatProperty(DetailPropertyContext& detailContext, const char* label, float value, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::TextDisabled("%.3f", value);
        ImGui::PopID();
    }

    void Draw_ReadOnlyCheckedBoolProperty(DetailPropertyContext& detailContext, const char* label, bool value, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::BeginDisabled();
        ImGui::Checkbox("##Value", &value);
        ImGui::EndDisabled();
        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
    }
}

EffectMaterialInstance_View::EffectMaterialInstance_View()
    : Editor_Window{ L"Effect Material Instance", ICON_FA_PALETTE }
    , _previewRenderer{ EffectMaterialPreviewRenderer::Create() }
{
}

EffectMaterialInstance_View::~EffectMaterialInstance_View()
{
    _textureThumbnailCache.clear();
}

void EffectMaterialInstance_View::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if (Is_Open() && _previewRenderer != nullptr)
        _previewRenderer->Update_MaterialTime(timeDelta);
}

void EffectMaterialInstance_View::Pre_Render()
{
    if (!Is_Open() || _previewRenderer == nullptr)
        return;

    Shared<Emitter_View> emitterView{};
    AuthoringEmitter* emitter{ nullptr };
    RequiredModuleData* requiredData{ nullptr };
    if (!Resolve_Target(emitterView, emitter, requiredData))
        return;

    const EffectMaterialPreviewDistortionContext distortionContext =
        emitter->typeData.kind == AuthoringTypeDataKind::Trail
        ? EffectMaterialPreviewDistortionContext::Trail
        : EffectMaterialPreviewDistortionContext::Generic;

    if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(*emitter))
    {
        const bool evaluateParticleLife =
            emitter->typeData.kind == AuthoringTypeDataKind::None && emitter->rendererType == "Sprite";
        _previewRenderer->Render_Material(
            requiredData->material,
            Build_CoreColorRgbModulationRuntimeDesc(*modulation),
            Build_MaterialVec2ModulationRuntimeDesc(*modulation),
            Build_MaterialScalarModulationRuntimeDesc(*modulation),
            requiredData->duration,
            evaluateParticleLife,
            distortionContext
        );
    }

    else
        _previewRenderer->Render_Material(requiredData->material, distortionContext);
}

void EffectMaterialInstance_View::Render()
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

        Shared<Emitter_View> emitterView{};
        AuthoringEmitter* emitter{ nullptr };
        RequiredModuleData* requiredData{ nullptr };
        if (!Resolve_Target(emitterView, emitter, requiredData))
        {
            Commit_PendingAuthoringEdit(emitterView);
            ImGui::TextDisabled("Material instance target is not selected.");
        }
        else
        {
            if (emitter->typeData.kind == AuthoringTypeDataKind::Mesh)
            {
                Commit_PendingAuthoringEdit(emitterView);
                ImGui::TextDisabled("Mesh Required material authoring is disabled.");
                ImGui::TextDisabled("Mesh materials are edited from Mesh TypeData Assigned Material.");
            }
            else
            {
                bool changed = false;
                bool restartPreview = false;
                const float contentWidth = ImGui::GetContentRegionAvail().x;
                const bool useSideBySideLayout = contentWidth >= 760.f;
                const EffectAuthoringSelection selection{
                    EffectAuthoringSelectionKind::Module,
                    _targetEmitterId,
                    _targetModuleId
                };
                const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
                const EffectMaterialTwoSidedDisplayMode twoSidedDisplayMode =
                    emitter->typeData.kind == AuthoringTypeDataKind::None
                    ? EffectMaterialTwoSidedDisplayMode::ForcedEnabledReadOnly
                    : emitter->typeData.kind == AuthoringTypeDataKind::Trail
                      || emitter->typeData.kind == AuthoringTypeDataKind::Ribbon
                      ? EffectMaterialTwoSidedDisplayMode::EditableUnimplemented
                      : EffectMaterialTwoSidedDisplayMode::EditableImplemented;
                const EffectMaterialDistortionShapeControlMode distortionShapeControlMode =
                    emitter->typeData.kind == AuthoringTypeDataKind::Trail
                    ? EffectMaterialDistortionShapeControlMode::TrailEditable
                    : emitter->typeData.kind == AuthoringTypeDataKind::Beam
                      ? EffectMaterialDistortionShapeControlMode::BeamUnsupported
                      : EffectMaterialDistortionShapeControlMode::Editable;
                const string previewLabel = Build_EmitterPreviewLabel(
                    emitterView->Get_Emitters(),
                    *emitter,
                    "Material Instance"
                );

                if (useSideBySideLayout)
                {
                    const float spacing = ImGui::GetStyle().ItemSpacing.x;
                    const float previewPaneWidth = clamp(
                        contentWidth * kPreviewPaneWidthRatio,
                        kPreviewPaneMinWidth,
                        kPreviewPaneMaxWidth
                    );

                    ImGui::BeginChild("EffectMaterialInstancePreviewPane", ImVec2(previewPaneWidth, 0.f), false);
                    {
                        if (_previewRenderer != nullptr)
                        {
                            _previewRenderer->Draw_Viewport(
                                "EffectMaterialInstancePreview",
                                requiredData->material,
                                previewLabel.c_str(),
                                true
                            );
                        }
                    }
                    ImGui::EndChild();

                    ImGui::SameLine(0.f, spacing);

                    ImGui::BeginChild("EffectMaterialInstancePropertyPane", ImVec2(0.f, 0.f), false);
                    {
                        changed |= Draw_InstanceProperties(
                            _detailPropertyContext,
                            _textureThumbnailCache,
                            requiredData->material,
                            &restartPreview,
                            twoSidedDisplayMode,
                            distortionShapeControlMode
                        );
                    }
                    ImGui::EndChild();
                }
                else
                {
                    if (_previewRenderer != nullptr)
                    {
                        _previewRenderer->Draw_Viewport(
                            "EffectMaterialInstancePreview",
                            requiredData->material,
                            previewLabel.c_str()
                        );
                        ImGui::Separator();
                    }

                    changed |= Draw_InstanceProperties(
                        _detailPropertyContext,
                        _textureThumbnailCache,
                        requiredData->material,
                        &restartPreview,
                        twoSidedDisplayMode,
                        distortionShapeControlMode
                    );
                }

                if (changed)
                {
                    Apply_MaterialChanged(emitterView, *emitter, *requiredData);
                    Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Material Instance");

                    if (restartPreview && EDITOR != nullptr)
                        EDITOR->Request_RestartPreview();
                }

                Commit_PendingAuthoringEditIfIdle(emitterView);
            }
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

bool EffectMaterialInstance_View::Draw_InstanceProperties(
    DetailPropertyContext& detailPropertyContext,
    map<string, Shared<Texture>>& textureThumbnailCache,
    EffectMaterialInstanceData& material,
    bool* outRestartPreview,
    EffectMaterialTwoSidedDisplayMode twoSidedDisplayMode,
    EffectMaterialDistortionShapeControlMode distortionShapeControlMode,
    const EffectMaterialRenderPolicyBinding* renderPolicyBinding,
    const char* mainTextureSourceLockTooltip)
{
    const EffectMaterialInstanceData defaultData{};
    const EffectMaterialBlendMode effectiveBlendMode =
        renderPolicyBinding != nullptr ? renderPolicyBinding->blendMode : material.blendMode;
    bool changed = false;
    bool restartPreview = false;

    constexpr const char* sourceOptions[]{ "Alpha", "Red", "Luminance" };

    if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMain"))
        {
            changed |= Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Main Texture",
                material.mainTextureGuid,
                material.mainTexturePath,
                defaultData.mainTextureGuid,
                defaultData.mainTexturePath,
                "최종 색과 opacity의 기본 texture입니다. 비워두면 material shader의 fallback 입력을 사용합니다.",
                mainTextureSourceLockTooltip
            );
            changed |= Draw_StringComboProperty(
                detailPropertyContext,
                "Opacity Source",
                material.opacitySource,
                defaultData.opacitySource,
                sourceOptions,
                std::size(sourceOptions),
                "Main Texture에서 opacity로 읽을 채널입니다. Alpha/Red/Luminance 중 효과 texture packing에 맞춰 고릅니다."
            );
            changed |= detailPropertyContext.Draw_ColorProperty(
                "Tint",
                material.tint,
                defaultData.tint,
                "Main RGB에 곱해지는 material tint입니다. emitter color/life color와 추가로 곱해집니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty("Intensity", material.intensity, defaultData.intensity, 0.01f, "최종 색 밝기에 곱하는 기본 강도입니다.");
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Opacity Power",
                material.opacityPower,
                defaultData.opacityPower,
                0.01f,
                "선택한 opacity 채널에 적용하는 power 보정입니다. 값이 커질수록 낮은 alpha가 더 빨리 줄어듭니다."
            );
            const bool mainModeChanged = Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Main UV Tiling Mode",
                material.mainUVTilingMode,
                defaultData.mainUVTilingMode,
                "Main texture의 U/V 반복 정책을 한 번에 맞추는 legacy 편의값입니다. 축별 정책을 바꾸면 그 값이 우선합니다."
            );
            changed |= mainModeChanged;
            restartPreview |= mainModeChanged;
            if (mainModeChanged)
            {
                material.mainUVPolicy.uPolicy = material.mainUVTilingMode;
                material.mainUVPolicy.vPolicy = material.mainUVTilingMode;
            }
            const bool mainAxisPolicyChanged = Draw_UVAxisPolicyProperties(
                detailPropertyContext,
                "Main U Policy",
                "Main V Policy",
                material.mainUVPolicy,
                defaultData.mainUVPolicy,
                material.mainUVTilingMode,
                "Main texture U축의 반복/늘림/클램프 정책입니다.",
                "Main texture V축의 반복/늘림/클램프 정책입니다."
            );
            changed |= mainAxisPolicyChanged;
            restartPreview |= mainAxisPolicyChanged;
            const bool mainRotationChanged = Draw_UVRotationProperty(
                detailPropertyContext,
                "Main UV Rotation",
                material.mainUVRotation,
                defaultData.mainUVRotation,
                "Main texture UV를 0.5 중심으로 90도 단위 회전한 뒤 scale/offset/scroll을 적용합니다."
            );
            changed |= mainRotationChanged;
            restartPreview |= mainRotationChanged;
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Main UV Scale",
                material.mainUVScale,
                defaultData.mainUVScale,
                0.01f,
                nullptr,
                "Main texture UV에 곱하는 타일 배율입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Main UV Offset",
                material.mainUVOffset,
                defaultData.mainUVOffset,
                0.01f,
                nullptr,
                "Main texture UV에 더하는 정적 오프셋입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Main UV Scroll Speed",
                material.mainUVScrollSpeed,
                defaultData.mainUVScrollSpeed,
                0.01f,
                nullptr,
                "초당 Main texture UV를 이동시키는 속도입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }

        if (!textureAssignStatus.empty())
            ImGui::TextDisabled("%s", textureAssignStatus.c_str());
    }

    if (ImGui::CollapsingHeader("Noise", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceNoise"))
        {
            changed |= Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Noise Texture",
                material.noiseTextureGuid,
                material.noiseTexturePath,
                defaultData.noiseTextureGuid,
                defaultData.noiseTexturePath,
                "opacity나 breakup에 섞을 noise texture입니다. Strength가 0이면 시각 영향이 거의 없습니다."
            );
            changed |= Draw_StringComboProperty(
                detailPropertyContext,
                "Noise Source",
                material.noiseSource,
                defaultData.noiseSource,
                sourceOptions,
                std::size(sourceOptions),
                "Noise Texture에서 modulation 값으로 읽을 채널입니다."
            );
            changed |= detailPropertyContext.Draw_BoolProperty(
                "Noise Invert",
                material.noiseInvert,
                defaultData.noiseInvert,
                "Noise 값을 뒤집어 밝은 영역과 어두운 영역의 역할을 바꿉니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Noise Strength",
                material.noiseStrength,
                defaultData.noiseStrength,
                0.01f,
                "Noise가 opacity/coverage에 주는 영향 강도입니다."
            );
            const bool noiseModeChanged = Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Noise UV Tiling Mode",
                material.noiseUVTilingMode,
                defaultData.noiseUVTilingMode,
                "Noise texture의 U/V 반복 정책을 한 번에 맞추는 legacy 편의값입니다."
            );
            changed |= noiseModeChanged;
            restartPreview |= noiseModeChanged;
            if (noiseModeChanged)
            {
                material.noiseUVPolicy.uPolicy = material.noiseUVTilingMode;
                material.noiseUVPolicy.vPolicy = material.noiseUVTilingMode;
            }
            const bool noiseAxisPolicyChanged = Draw_UVAxisPolicyProperties(
                detailPropertyContext,
                "Noise U Policy",
                "Noise V Policy",
                material.noiseUVPolicy,
                defaultData.noiseUVPolicy,
                material.noiseUVTilingMode,
                "Noise texture U축의 반복/늘림/클램프 정책입니다.",
                "Noise texture V축의 반복/늘림/클램프 정책입니다."
            );
            changed |= noiseAxisPolicyChanged;
            restartPreview |= noiseAxisPolicyChanged;
            const bool noiseRotationChanged = Draw_UVRotationProperty(
                detailPropertyContext,
                "Noise UV Rotation",
                material.noiseUVRotation,
                defaultData.noiseUVRotation,
                "Noise texture UV를 0.5 중심으로 90도 단위 회전한 뒤 scale/offset/scroll을 적용합니다."
            );
            changed |= noiseRotationChanged;
            restartPreview |= noiseRotationChanged;
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Noise UV Scale",
                material.noiseUVScale,
                defaultData.noiseUVScale,
                0.01f,
                nullptr,
                "Noise texture UV에 곱하는 타일 배율입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Noise UV Offset",
                material.noiseUVOffset,
                defaultData.noiseUVOffset,
                0.01f,
                nullptr,
                "Noise texture UV에 더하는 정적 오프셋입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Noise UV Scroll Speed",
                material.noiseUVScrollSpeed,
                defaultData.noiseUVScrollSpeed,
                0.01f,
                nullptr,
                "초당 Noise texture UV를 이동시키는 속도입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("Mask", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMask"))
        {
            changed |= Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Mask Texture",
                material.maskTextureGuid,
                material.maskTexturePath,
                defaultData.maskTextureGuid,
                defaultData.maskTexturePath,
                "coverage를 자르거나 erosion에 사용할 mask texture입니다."
            );
            changed |= Draw_StringComboProperty(
                detailPropertyContext,
                "Mask Source",
                material.maskSource,
                defaultData.maskSource,
                sourceOptions,
                std::size(sourceOptions),
                "Mask Texture에서 coverage 값으로 읽을 채널입니다."
            );
            changed |= detailPropertyContext.Draw_BoolProperty("Mask Invert", material.maskInvert, defaultData.maskInvert, "Mask 값을 뒤집어 남는 영역과 지워지는 영역을 바꿉니다.");
            if (renderPolicyBinding != nullptr)
            {
                Draw_ReadOnlyFloatProperty(
                    detailPropertyContext,
                    "Alpha Cutoff",
                    renderPolicyBinding->alphaCutoff,
                    "현재 cutoff 정본은 Render Policy Source가 가리키는 Required 또는 Assigned Material 값입니다."
                );
            }
            else
            {
                changed |= detailPropertyContext.Draw_FloatProperty(
                    "Alpha Cutoff",
                    material.alphaCutoff,
                    defaultData.alphaCutoff,
                    0.01f,
                    "Masked blend에서 버릴 opacity 임계값입니다."
                );
            }
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Alpha Erosion",
                material.alphaErosion,
                defaultData.alphaErosion,
                0.01f,
                "Mask/opacity coverage를 안쪽으로 깎아내는 erosion 강도입니다."
            );
            const bool maskModeChanged = Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Mask UV Tiling Mode",
                material.maskUVTilingMode,
                defaultData.maskUVTilingMode,
                "Mask texture의 U/V 반복 정책을 한 번에 맞추는 legacy 편의값입니다."
            );
            changed |= maskModeChanged;
            restartPreview |= maskModeChanged;
            if (maskModeChanged)
            {
                material.maskUVPolicy.uPolicy = material.maskUVTilingMode;
                material.maskUVPolicy.vPolicy = material.maskUVTilingMode;
            }
            const bool maskAxisPolicyChanged = Draw_UVAxisPolicyProperties(
                detailPropertyContext,
                "Mask U Policy",
                "Mask V Policy",
                material.maskUVPolicy,
                defaultData.maskUVPolicy,
                material.maskUVTilingMode,
                "Mask texture U축의 반복/늘림/클램프 정책입니다.",
                "Mask texture V축의 반복/늘림/클램프 정책입니다."
            );
            changed |= maskAxisPolicyChanged;
            restartPreview |= maskAxisPolicyChanged;
            const bool maskRotationChanged = Draw_UVRotationProperty(
                detailPropertyContext,
                "Mask UV Rotation",
                material.maskUVRotation,
                defaultData.maskUVRotation,
                "Mask texture UV를 0.5 중심으로 90도 단위 회전한 뒤 scale/offset/scroll을 적용합니다."
            );
            changed |= maskRotationChanged;
            restartPreview |= maskRotationChanged;
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Mask UV Scale",
                material.maskUVScale,
                defaultData.maskUVScale,
                0.01f,
                nullptr,
                "Mask texture UV에 곱하는 타일 배율입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Mask UV Offset",
                material.maskUVOffset,
                defaultData.maskUVOffset,
                0.01f,
                nullptr,
                "Mask texture UV에 더하는 정적 오프셋입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Mask UV Scroll Speed",
                material.maskUVScrollSpeed,
                defaultData.maskUVScrollSpeed,
                0.01f,
                nullptr,
                "초당 Mask texture UV를 이동시키는 속도입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (material.materialFamily == EffectMaterialFamily::SpriteDistortion &&
        ImGui::CollapsingHeader("Flow / Distortion", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceDistortion"))
        {
            changed |= Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Flow Texture",
                material.flowTextureGuid,
                material.flowTexturePath,
                defaultData.flowTextureGuid,
                defaultData.flowTexturePath,
                "SpriteDistortion에서 화면 굴절 방향과 세기를 읽는 flow texture입니다."
            );
            const bool flowModeChanged = Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Flow UV Tiling Mode",
                material.flowUVTilingMode,
                defaultData.flowUVTilingMode,
                "Flow texture의 U/V 반복 정책을 한 번에 맞추는 legacy 편의값입니다."
            );
            changed |= flowModeChanged;
            restartPreview |= flowModeChanged;
            if (flowModeChanged)
            {
                material.flowUVPolicy.uPolicy = material.flowUVTilingMode;
                material.flowUVPolicy.vPolicy = material.flowUVTilingMode;
            }
            const bool flowAxisPolicyChanged = Draw_UVAxisPolicyProperties(
                detailPropertyContext,
                "Flow U Policy",
                "Flow V Policy",
                material.flowUVPolicy,
                defaultData.flowUVPolicy,
                material.flowUVTilingMode,
                "Flow texture U축의 반복/늘림/클램프 정책입니다.",
                "Flow texture V축의 반복/늘림/클램프 정책입니다."
            );
            changed |= flowAxisPolicyChanged;
            restartPreview |= flowAxisPolicyChanged;
            const bool flowRotationChanged = Draw_UVRotationProperty(
                detailPropertyContext,
                "Flow UV Rotation",
                material.flowUVRotation,
                defaultData.flowUVRotation,
                "Flow texture UV를 0.5 중심으로 90도 단위 회전한 뒤 scale/offset/scroll을 적용합니다."
            );
            changed |= flowRotationChanged;
            restartPreview |= flowRotationChanged;
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Flow UV Scale",
                material.flowUVScale,
                defaultData.flowUVScale,
                0.01f,
                nullptr,
                "Flow texture UV에 곱하는 타일 배율입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Flow UV Offset",
                material.flowUVOffset,
                defaultData.flowUVOffset,
                0.01f,
                nullptr,
                "Flow texture UV에 더하는 정적 오프셋입니다."
            );
            changed |= detailPropertyContext.Draw_Vec2Property(
                "Flow UV Scroll Speed",
                material.flowUVScrollSpeed,
                defaultData.flowUVScrollSpeed,
                0.01f,
                nullptr,
                "초당 Flow texture UV를 이동시키는 속도입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Refraction Intensity",
                material.refractionIntensity,
                defaultData.refractionIntensity,
                0.01f,
                "flow vector가 화면 샘플을 얼마나 밀어낼지 정하는 굴절 강도입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Refraction Presence",
                material.refractionPresence,
                defaultData.refractionPresence,
                0.01f,
                "굴절 효과가 나타나는 기본 존재감/coverage입니다."
            );
            const bool shapeEditable =
                distortionShapeControlMode == EffectMaterialDistortionShapeControlMode::Editable ||
                distortionShapeControlMode == EffectMaterialDistortionShapeControlMode::TrailEditable;
            if (!shapeEditable)
            {
                const char* unsupportedLabel =
                    distortionShapeControlMode == EffectMaterialDistortionShapeControlMode::BeamUnsupported
                    ? "Beam Distortion에서는 Shape 미지원"
                    : "Mesh Distortion v0에서는 Shape 미지원";
                Draw_ReadOnlyTextProperty(
                    detailPropertyContext,
                    "Shape Mode",
                    unsupportedLabel,
                    "현재 renderer 조합에서는 distortion shape mask를 개별 authoring하지 않습니다."
                );
                detailPropertyContext.End_PropertyTable();
            }
            else
            {
                const EffectDistortionShapeMode previousShapeMode = material.distortionShapeMode;
                EffectDistortionShapeMode displayedShapeMode = material.distortionShapeMode;
                if (distortionShapeControlMode == EffectMaterialDistortionShapeControlMode::TrailEditable)
                {
                    if (displayedShapeMode != EffectDistortionShapeMode::AirSheath)
                        displayedShapeMode = EffectDistortionShapeMode::None;
                }
                else if (displayedShapeMode == EffectDistortionShapeMode::AirSheath)
                    displayedShapeMode = EffectDistortionShapeMode::None;

                if (displayedShapeMode != material.distortionShapeMode)
                {
                    const char* compatibilityMessage =
                        distortionShapeControlMode == EffectMaterialDistortionShapeControlMode::TrailEditable
                        ? "저장된 Sprite shape는 Trail에서 None으로 처리됩니다."
                        : "AirSheath는 Trail 전용이며 현재 renderer에서는 None으로 처리됩니다.";
                    Draw_ReadOnlyTextProperty(
                        detailPropertyContext,
                        "Shape Compatibility",
                        compatibilityMessage
                    );
                }

                const bool shapeModeChanged = Draw_DistortionShapeModeProperty(
                    detailPropertyContext,
                    displayedShapeMode,
                    defaultData.distortionShapeMode,
                    distortionShapeControlMode
                );
                changed |= shapeModeChanged;
                if (shapeModeChanged)
                    material.distortionShapeMode = displayedShapeMode;

                if (shapeModeChanged &&
                    previousShapeMode != EffectDistortionShapeMode::ShockwaveRing &&
                    material.distortionShapeMode == EffectDistortionShapeMode::ShockwaveRing)
                {
                    material.distortionShapeThickness = 14.f;
                    material.distortionShapeSoftness = 4.f;
                }
                if (displayedShapeMode == EffectDistortionShapeMode::Radial ||
                    displayedShapeMode == EffectDistortionShapeMode::Ring)
                {
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Radius",
                        material.distortionShapeRadius,
                        defaultData.distortionShapeRadius,
                        0.01f,
                        "Radial/Ring shape가 영향을 주는 중심 반경입니다."
                    );
                    if (displayedShapeMode == EffectDistortionShapeMode::Ring)
                    {
                        changed |= detailPropertyContext.Draw_FloatProperty(
                            "Shape Thickness",
                            material.distortionShapeThickness,
                            defaultData.distortionShapeThickness,
                            0.01f,
                            "Ring shape의 굴절 띠 두께입니다."
                        );
                    }
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Softness",
                        material.distortionShapeSoftness,
                        defaultData.distortionShapeSoftness,
                        0.01f,
                        "shape 경계가 부드럽게 사라지는 폭입니다."
                    );
                }
                else if (displayedShapeMode == EffectDistortionShapeMode::ShockwaveRing)
                {
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Thickness (px)",
                        material.distortionShapeThickness,
                        defaultData.distortionShapeThickness,
                        0.01f,
                        "Shockwave ring의 화면 픽셀 기준 띠 두께입니다."
                    );
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Softness (px)",
                        material.distortionShapeSoftness,
                        defaultData.distortionShapeSoftness,
                        0.01f,
                        "Shockwave ring 경계가 화면 픽셀 기준으로 부드러워지는 폭입니다."
                    );
                }
                else if (displayedShapeMode == EffectDistortionShapeMode::AirSheath)
                {
                    static constexpr EffectAirSheathMapInterpretation kInterpretations[]{
                        EffectAirSheathMapInterpretation::VectorField,
                        EffectAirSheathMapInterpretation::HeightGradient,
                        EffectAirSheathMapInterpretation::ScalarModulation
                    };
                    static constexpr EffectAirSheathMapScalarSource kScalarSources[]{
                        EffectAirSheathMapScalarSource::Red,
                        EffectAirSheathMapScalarSource::Green,
                        EffectAirSheathMapScalarSource::Blue,
                        EffectAirSheathMapScalarSource::Alpha,
                        EffectAirSheathMapScalarSource::Luminance
                    };
                    static constexpr EffectAirSheathMapVectorSpace kVectorSpaces[]{
                        EffectAirSheathMapVectorSpace::Screen,
                        EffectAirSheathMapVectorSpace::TrailLocal,
                        EffectAirSheathMapVectorSpace::BaseField
                    };
                    static constexpr EffectAirSheathMapComposition kVectorCompositions[]{
                        EffectAirSheathMapComposition::Replace,
                        EffectAirSheathMapComposition::Add
                    };
                    static constexpr EffectAirSheathMapComposition kScalarCompositions[]{
                        EffectAirSheathMapComposition::Modulate
                    };

                    changed |= Draw_EnumComboProperty(
                        detailPropertyContext,
                        "Interpretation",
                        material.airSheathMapInterpretation,
                        defaultData.airSheathMapInterpretation,
                        kInterpretations,
                        size(kInterpretations),
                        Get_AirSheathMapInterpretationLabel,
                        "Distortion Map을 vector, height gradient, 또는 base wake 강도 조절로 해석합니다.");
                    const bool scalarModulation = material.airSheathMapInterpretation == EffectAirSheathMapInterpretation::ScalarModulation;
                    if (scalarModulation)
                    {
                        material.airSheathMapComposition = EffectAirSheathMapComposition::Modulate;
                        changed |= Draw_EnumComboProperty(
                            detailPropertyContext,
                            "Scalar Source",
                            material.airSheathMapXSource,
                            defaultData.airSheathMapXSource,
                            kScalarSources,
                            size(kScalarSources),
                            Get_AirSheathMapScalarSourceLabel,
                            "0~1 scalar가 base wake 강도를 조절합니다.");
                    }
                    else
                    {
                        if (material.airSheathMapComposition == EffectAirSheathMapComposition::Modulate)
                            material.airSheathMapComposition = EffectAirSheathMapComposition::Replace;
                        changed |= Draw_EnumComboProperty(
                            detailPropertyContext,
                            material.airSheathMapInterpretation == EffectAirSheathMapInterpretation::VectorField ? "X Source" : "Height Source",
                            material.airSheathMapXSource,
                            defaultData.airSheathMapXSource,
                            kScalarSources,
                            size(kScalarSources),
                            Get_AirSheathMapScalarSourceLabel);
                        if (material.airSheathMapInterpretation == EffectAirSheathMapInterpretation::VectorField)
                            changed |= Draw_EnumComboProperty(
                                detailPropertyContext,
                                "Y Source",
                                material.airSheathMapYSource,
                                defaultData.airSheathMapYSource,
                                kScalarSources,
                                size(kScalarSources),
                                Get_AirSheathMapScalarSourceLabel);
                        changed |= Draw_EnumComboProperty(
                            detailPropertyContext,
                            "Vector Space",
                            material.airSheathMapVectorSpace,
                            defaultData.airSheathMapVectorSpace,
                            kVectorSpaces,
                            size(kVectorSpaces),
                            Get_AirSheathMapVectorSpaceLabel,
                            "Screen은 화면 축, Trail Local/Base Field는 투영된 진행/폭 축을 사용합니다.");
                    }
                    changed |= Draw_EnumComboProperty(
                        detailPropertyContext,
                        "Composition",
                        material.airSheathMapComposition,
                        defaultData.airSheathMapComposition,
                        scalarModulation ? kScalarCompositions : kVectorCompositions,
                        scalarModulation ? size(kScalarCompositions) : size(kVectorCompositions),
                        Get_AirSheathMapCompositionLabel,
                        "Vector/gradient는 Replace 또는 Add, scalar는 Modulate만 사용할 수 있습니다.");
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Map Influence",
                        material.airSheathMapInfluence,
                        defaultData.airSheathMapInfluence,
                        0.01f,
                        "0이면 map 합성 없이 AirSheath base wake만 사용합니다.");
                }
                detailPropertyContext.End_PropertyTable();
            }
        }
    }
    else if (material.materialFamily != EffectMaterialFamily::SpriteDistortion)
        ImGui::TextDisabled("Flow / Distortion fields are shown only when the embedded material family is SpriteDistortion.");

    if (material.materialFamily == EffectMaterialFamily::MeshGlass &&
        ImGui::CollapsingHeader("Mesh Glass", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMeshGlassSurface"))
        {
            Draw_ReadOnlyTextProperty(
                detailPropertyContext,
                "Mode",
                "Mesh surface glass",
                "MeshGlass는 mesh 표면의 기본 투명도, view-angle rim, fake light highlight를 사용하는 전용 family입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Glass Alpha",
                material.glassAlpha,
                defaultData.glassAlpha,
                0.01f,
                "MeshGlass 표면의 기본 alpha입니다. 낮을수록 거의 투명한 유리처럼 보입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Alpha Power",
                material.glassAlphaPower,
                defaultData.glassAlphaPower,
                0.01f,
                "Main/Mask/Noise coverage를 날카롭게 하거나 부드럽게 하는 보정입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Normal Strength",
                material.glassNormalStrength,
                defaultData.glassNormalStrength,
                0.01f,
                "model normal texture가 Glass rim/highlight 방향에 주는 영향 강도입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }

        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMeshGlassRim"))
        {
            changed |= detailPropertyContext.Draw_ColorProperty(
                "Rim Color",
                material.glassRimColor,
                defaultData.glassRimColor,
                "view angle이 얕은 외곽 rim 반사의 색입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Rim Intensity",
                material.glassRimIntensity,
                defaultData.glassRimIntensity,
                0.01f,
                "외곽 rim 반사의 밝기 배율입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Rim Power",
                material.glassRimPower,
                defaultData.glassRimPower,
                0.01f,
                "외곽 rim falloff의 날카로움입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }

        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMeshGlassFakeLight"))
        {
            changed |= detailPropertyContext.Draw_Vec3Property(
                "Light Direction",
                material.glassLightDirection,
                defaultData.glassLightDirection,
                0.01f,
                nullptr,
                "MeshGlass fake highlight에 사용하는 world-space light direction입니다."
            );
            changed |= detailPropertyContext.Draw_ColorProperty(
                "Light Color",
                material.glassLightColor,
                defaultData.glassLightColor,
                "fake light highlight 색입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Light Intensity",
                material.glassLightIntensity,
                defaultData.glassLightIntensity,
                0.01f,
                "fake light highlight 밝기 배율입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Specular Power",
                material.glassSpecularPower,
                defaultData.glassSpecularPower,
                0.01f,
                "fake highlight의 날카로움입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Specular Softness",
                material.glassSpecularSoftness,
                defaultData.glassSpecularSoftness,
                0.01f,
                "fake highlight 폭을 넓히는 보정입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }

        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMeshGlassTextureInfluence"))
        {
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Main Influence",
                material.glassMainInfluence,
                defaultData.glassMainInfluence,
                0.01f,
                "Main Texture가 glass tint/pattern에 섞이는 강도입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Noise Breakup",
                material.glassNoiseBreakup,
                defaultData.glassNoiseBreakup,
                0.01f,
                "Noise Texture가 alpha/rim/specular breakup에 섞이는 강도입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Mask Strength",
                material.glassMaskStrength,
                defaultData.glassMaskStrength,
                0.01f,
                "Mask Texture가 Glass coverage에 주는 강도입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("Blend / Render", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceBlendRender"))
        {
            const bool familyChanged = Draw_MaterialFamilyProperty(
                detailPropertyContext,
                material.materialFamily,
                defaultData.materialFamily
            );
            changed |= familyChanged;
            restartPreview |= familyChanged;
            if (renderPolicyBinding != nullptr)
            {
                Draw_ReadOnlyTextProperty(
                    detailPropertyContext,
                    "Render Policy Source",
                    renderPolicyBinding->sourceLabel,
                    "Blend Mode와 Alpha Cutoff의 현재 정본 위치입니다. MeshData에서는 Required가 override할 수 있습니다."
                );
                Draw_ReadOnlyTextProperty(
                    detailPropertyContext,
                    "Blend Mode",
                    Get_BlendModeLabel(renderPolicyBinding->blendMode),
                    "현재 blend mode는 Render Policy Source가 가리키는 값에서 옵니다."
                );
            }
            else
            {
                if (material.materialFamily == EffectMaterialFamily::MeshGlass)
                {
                    Draw_ReadOnlyTextProperty(
                        detailPropertyContext,
                        "Blend Mode",
                        "AlphaBlend (MeshGlass v0)",
                        "MeshGlass v0는 저장된 Blend Mode를 보존하지만 preview/runtime에서는 AlphaBlend로 렌더합니다."
                    );
                }
                else
                {
                    changed |= Draw_BlendModeProperty(
                        detailPropertyContext,
                        material.blendMode,
                        defaultData.blendMode,
                        "투명/가산/마스크 렌더 경로를 고르는 material render 정책입니다."
                    );
                }
            }
            switch (twoSidedDisplayMode)
            {
            case EffectMaterialTwoSidedDisplayMode::EditableImplemented:
                changed |= detailPropertyContext.Draw_BoolProperty("Two Sided", material.twoSided, defaultData.twoSided, "Mesh material을 양면 렌더할지 정합니다.");
                break;

            case EffectMaterialTwoSidedDisplayMode::ForcedEnabledReadOnly:
                Draw_ReadOnlyCheckedBoolProperty(
                    detailPropertyContext,
                    "Two Sided",
                    true,
                    "현재 sprite renderer는 항상 양면 렌더라서 이 옵션을 개별 제어하지 않습니다."
                );
                break;

            case EffectMaterialTwoSidedDisplayMode::EditableUnimplemented:
            default:
                changed |= detailPropertyContext.Draw_BoolProperty(
                    "Two Sided (미구현)",
                    material.twoSided,
                    defaultData.twoSided,
                    "현재 renderer에서는 저장만 되고 실제 양면 제어는 아직 적용되지 않습니다."
                );
                break;
            }
            changed |= detailPropertyContext.Draw_UintProperty("SubUV Cols", material.subUVCols, defaultData.subUVCols, "SubUV atlas의 가로 frame 개수입니다.");
            changed |= detailPropertyContext.Draw_UintProperty("SubUV Rows", material.subUVRows, defaultData.subUVRows, "SubUV atlas의 세로 frame 개수입니다.");
            detailPropertyContext.End_PropertyTable();
        }

        if (effectiveBlendMode == EffectMaterialBlendMode::Additive)
            changed |= Draw_AdditiveContributionPanel(detailPropertyContext, material.additive, defaultData.additive);

        if (twoSidedDisplayMode == EffectMaterialTwoSidedDisplayMode::ForcedEnabledReadOnly)
            ImGui::TextDisabled("Sprite는 현재 항상 양면 렌더입니다.");
    }

    if (ImGui::CollapsingHeader("Core Emissive", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceCoreEmissive"))
        {
            changed |= detailPropertyContext.Draw_BoolProperty(
                "Enabled",
                material.coreEmissive.enabled,
                defaultData.coreEmissive.enabled,
                "중심부와 외곽 emissive 보정을 추가로 적용합니다."
            );
            changed |= detailPropertyContext.Draw_ColorRgbProperty(
                "Core Color",
                material.coreEmissive.coreColor,
                defaultData.coreEmissive.coreColor,
                "중심 emissive에 사용할 RGB 색입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Core Power",
                material.coreEmissive.corePower,
                defaultData.coreEmissive.corePower,
                0.01f,
                "중심부 falloff 곡선의 날카로움입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Core Intensity",
                material.coreEmissive.coreIntensity,
                defaultData.coreEmissive.coreIntensity,
                0.01f,
                "중심 emissive 밝기 배율입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Outer Power",
                material.coreEmissive.outerPower,
                defaultData.coreEmissive.outerPower,
                0.01f,
                "외곽 emissive falloff 곡선의 날카로움입니다."
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Outer Intensity",
                material.coreEmissive.outerIntensity,
                defaultData.coreEmissive.outerIntensity,
                0.01f,
                "외곽 emissive 밝기 배율입니다."
            );
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (outRestartPreview != nullptr)
        *outRestartPreview = restartPreview;

    return changed;
}

void EffectMaterialInstance_View::Open_Instance(uint32 emitterId, uint32 moduleId)
{
    _targetEmitterId = emitterId;
    _targetModuleId = moduleId;
    Set_Open(true);
    Request_FocusOnOpen();
}

bool EffectMaterialInstance_View::Resolve_Target(
    Shared<Emitter_View>& outEmitterView,
    AuthoringEmitter*& outEmitter,
    RequiredModuleData*& outRequiredData) const
{
    outEmitterView.reset();
    outEmitter = nullptr;
    outRequiredData = nullptr;

    if (EDITOR == nullptr)
        return false;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    outEmitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (outEmitterView == nullptr)
        return false;

    outEmitter = outEmitterView->Find_Emitter(_targetEmitterId);
    if (outEmitter == nullptr)
        return false;

    AuthoringModule* module = outEmitterView->Find_Module(_targetEmitterId, _targetModuleId);
    if (module == nullptr || module->type != AuthoringModuleType::Required)
        return false;

    outRequiredData = get_if<RequiredModuleData>(&module->data);
    return outRequiredData != nullptr;
}

void EffectMaterialInstance_View::Apply_MaterialChanged(
    const Shared<Emitter_View>& emitterView,
    AuthoringEmitter& emitter,
    const RequiredModuleData& requiredData)
{
    emitter.previewDirty = true;
    emitter.rendererType = emitter.typeData.kind == AuthoringTypeDataKind::None
                           ? requiredData.rendererType
                           : Authoring::Resolve_RendererType(emitter.typeData);
    emitter.textureId = requiredData.material.mainTexturePath;
    emitter.resourceSummary = requiredData.material.mainTexturePath;

    if (emitterView)
        emitterView->MarkDirty();

    MarkDirty();
}

void EffectMaterialInstance_View::Begin_PendingAuthoringEdit(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    const Emitter_View::AuthoringSnapshot& beforeSnapshot,
    const string& description)
{
    if (emitterView == nullptr)
        return;

    if (!_hasPendingAuthoringEdit)
    {
        _hasPendingAuthoringEdit = true;
        _pendingAuthoringSelection = selection;
        _pendingAuthoringSnapshot = beforeSnapshot;
        _pendingAuthoringDescription = description;
    }
}

void EffectMaterialInstance_View::Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void EffectMaterialInstance_View::Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || emitterView == nullptr)
        return;

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView->Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = _pendingAuthoringSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = _pendingAuthoringSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = _pendingAuthoringSnapshot.selectedModuleIndex;
    emitterView->Execute_AuthoringSnapshotCommand(
        _pendingAuthoringSnapshot,
        afterSnapshot,
        _pendingAuthoringDescription
    );

    _hasPendingAuthoringEdit = false;
    _pendingAuthoringSelection = {};
    _pendingAuthoringSnapshot = {};
    _pendingAuthoringDescription.clear();
}

Shared<EffectMaterialInstance_View> EffectMaterialInstance_View::Create()
{
    return make_shared<EffectMaterialInstance_View>();
}

NS_END
