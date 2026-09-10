#include "Emitter_View.h"

#include "Action_Command.h"
#include "CurveEditor_View.h"
#include "Editor_Context.h"
#include "EffectAuthoringJsonSerializer.h"
#include "EffectAuthoringModuleMetadata.h"
#include "EffectAuthoring_Types.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPresetReader.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_ImGui.h"
#include "Helper_String.h"
#include "Notification_Manager.h"

#include <ctime>
#include <iomanip>
#include <sstream>

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kSoloPreviewIcon = ICON_FA_BULLSEYE;
    constexpr auto kPreviewTransformIcon = ICON_FA_UP_DOWN_LEFT_RIGHT;
    constexpr ImU32 kCurveEditorFocusBarColor = IM_COL32(80, 170, 205, 255);

    bool Uses_ParticleEmitterSource(const AuthoringEmitter& emitter)
    {
        if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon)
        {
            const RibbonTypeData* ribbonData = get_if<RibbonTypeData>(&emitter.typeData.payload);
            return ribbonData != nullptr && ribbonData->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter;
        }

        if (emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
        {
            const SourceHistorySpriteTrailTypeData* spriteTrailData =
                get_if<SourceHistorySpriteTrailTypeData>(&emitter.typeData.payload);
            return spriteTrailData != nullptr && spriteTrailData->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter;
        }

        return false;
    }

    const char* Build_PreviewTransformTooltip(const AuthoringEmitter& emitter, bool canUse)
    {
        if (!canUse)
        {
            if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon ||
                emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
                return "SourceHistory 계열은 source sample history가 정본입니다.\nSelfRoot/ParticleEmitter/Preview Object source 정책에 따라 움직임이 결정되므로 PT를 비활성화합니다.";

            return "Trail source는 Preview Object의 base/tip sample이 정본입니다.\nEmitter root transform은 Trail source 이동 테스트가 아니므로 PT를 비활성화합니다.";
        }

        return "저장되지 않는 Preview Transform입니다.\nScene 뷰 gizmo로 emitter 위치/회전을 프리뷰에서만 조정합니다.\nRestart Preview 시 authoring 값으로 돌아갑니다.";
    }
}

string Emitter_View::Build_ModuleWarningTooltip(const AuthoringEmitter& emitter, const AuthoringModule& module) const
{
    if (!emitter.enabled)
        return "비활성화된 이미터입니다.";

    if (!module.enabled && module.type != AuthoringModuleType::Required)
        return "비활성화된 모듈입니다.";

    if (!Is_ModuleCompatibleWithEmitter(emitter, module.type))
        return "현재 emitter 타입에서는 이 모듈이 preview에 반영되지 않습니다.";

    return {};
}

void Emitter_View::Draw_EmitterContextMenu(size_t emitterIndex)
{
    string emitterPasteReason{};
    const bool canPasteEmitter = Can_PasteEmitterAfter(emitterIndex, &emitterPasteReason);

    if (ImGui::BeginMenu("이미터"))
    {
        if (ImGui::MenuItem("이미터 복사"))
            Copy_EmitterToClipboard(emitterIndex);

        if (!canPasteEmitter)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("뒤에 이미터 붙여넣기"))
            Queue_ClipboardAction(PendingClipboardAction::PasteEmitterAfter, emitterIndex);
        if (!canPasteEmitter)
            ImGui::EndDisabled();
        if (!canPasteEmitter && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !emitterPasteReason.empty())
            ImGui::SetTooltip("%s", emitterPasteReason.c_str());

        if (ImGui::MenuItem("이미터 복제"))
            Queue_EmitterAction(PendingEmitterAction::Duplicate, emitterIndex);

        if (ImGui::MenuItem("이미터 삭제"))
            Queue_EmitterAction(PendingEmitterAction::Delete, emitterIndex);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("새 이미터"))
    {
        if (ImGui::MenuItem("앞에 새 이미터 추가"))
            Queue_EmitterAction(PendingEmitterAction::InsertBefore, emitterIndex);

        if (ImGui::MenuItem("뒤에 새 이미터 추가"))
            Queue_EmitterAction(PendingEmitterAction::InsertAfter, emitterIndex);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("타입 데이터"))
    {
        Draw_TypeDataContextMenu(emitterIndex);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("모듈"))
    {
        if (ImGui::MenuItem("새 모듈"))
            Open_ModulePicker(emitterIndex);

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("모든 이미터 활성화"))
        Set_AllEmittersEnabled(true);

    if (ImGui::MenuItem("모든 이미터 비활성화"))
        Set_AllEmittersEnabled(false);

    ImGui::Separator();

    const bool soloPreviewActive = Is_SoloPreviewActive();
    if (!soloPreviewActive)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("Solo 해제") && soloPreviewActive)
    {
        Clear_SoloPreviewEmitter();
        if (EDITOR != nullptr)
            EDITOR->Request_RestartPreview();
    }
    if (!soloPreviewActive)
        ImGui::EndDisabled();
}

void Emitter_View::Draw_TypeDataContextMenu(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    const AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (emitter.typeData.kind == AuthoringTypeDataKind::None)
    {
        if (ImGui::MenuItem("Trail TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddTrail, emitterIndex);
        if (ImGui::MenuItem("Mesh TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddMesh, emitterIndex);
        if (ImGui::MenuItem("Ribbon TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddRibbon, emitterIndex);
        if (ImGui::MenuItem("Sprite Trail TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddSourceHistorySpriteTrail, emitterIndex);
        if (ImGui::MenuItem("Beam TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddBeam, emitterIndex);
        return;
    }

    if (ImGui::MenuItem("타입 데이터 값 초기화"))
        Queue_TypeDataAction(PendingTypeDataAction::ResetData, emitterIndex);

    if (ImGui::MenuItem("타입 데이터 제거"))
        Queue_TypeDataAction(PendingTypeDataAction::Remove, emitterIndex);
}

void Emitter_View::Draw_ModuleContextMenu(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module)
{
    string modulePasteReason{};
    const bool canPasteModule = Can_PasteModuleInto(emitterIndex, &modulePasteReason);
    string valuePasteReason{};
    const bool canPasteValues = Can_PasteModuleValues(emitterIndex, moduleIndex, &valuePasteReason);

    if (ImGui::MenuItem("모듈 복사"))
        Copy_ModuleToClipboard(emitterIndex, moduleIndex);

    if (!canPasteModule)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("모듈 붙여넣기"))
        Queue_ClipboardAction(PendingClipboardAction::PasteModule, emitterIndex);
    if (!canPasteModule)
        ImGui::EndDisabled();
    if (!canPasteModule && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !modulePasteReason.empty())
        ImGui::SetTooltip("%s", modulePasteReason.c_str());

    if (!canPasteValues)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("값 붙여넣기"))
        Queue_ClipboardAction(PendingClipboardAction::PasteModuleValues, emitterIndex, moduleIndex);
    if (!canPasteValues)
        ImGui::EndDisabled();
    if (!canPasteValues && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !valuePasteReason.empty())
        ImGui::SetTooltip("%s", valuePasteReason.c_str());

    ImGui::Separator();

    if (ImGui::MenuItem("모듈 값 초기화"))
        Queue_ModuleAction(PendingModuleAction::ResetData, emitterIndex, moduleIndex);

    if (!module.removable)
        ImGui::BeginDisabled();

    if (ImGui::MenuItem("모듈 삭제"))
        Queue_ModuleAction(PendingModuleAction::Delete, emitterIndex, moduleIndex);

    if (!module.removable)
        ImGui::EndDisabled();
}

void Emitter_View::Draw_EmitterBoard()
{
    if (ImGui::BeginChild("##EmitterBoard", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        const ImVec2 boardMin = ImGui::GetWindowPos();
        const ImVec2 boardMax{
            boardMin.x + ImGui::GetWindowWidth(),
            boardMin.y + ImGui::GetWindowHeight()
        };
        const float columnHeight = max(1.f, ImGui::GetContentRegionAvail().y);

        for (size_t emitterIndex = 0; emitterIndex < _emitters.size(); ++emitterIndex)
        {
            ImGui::PushID(static_cast<int>(emitterIndex));
            Draw_EmitterColumn(emitterIndex, _emitters[emitterIndex], columnHeight);
            ImGui::PopID();

            if (emitterIndex + 1 < _emitters.size())
                ImGui::SameLine(0.f, 0.f);
        }

        if (!_emitters.empty())
            ImGui::SameLine(0.f, 0.f);

        ImGui::Dummy(ImVec2(kEmitterRightBlankWidth, columnHeight));
        const bool isRightGutterHovered = ImGui::IsItemHovered();
        const float blankRegionLeft = ImGui::GetItemRectMax().x;

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool isTrailingBlankHovered =
            ImGui::IsWindowHovered() &&
            !ImGui::IsAnyItemHovered() &&
            mousePos.x > blankRegionLeft &&
            mousePos.x < boardMax.x &&
            mousePos.y >= boardMin.y &&
            mousePos.y < boardMax.y;
        const bool isRightBlankHovered = isRightGutterHovered || isTrailingBlankHovered;

        if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            Clear_Selection();
        else if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            Clear_Selection();
            ImGui::OpenPopup("##EmitterBoardBlankPopup");
        }

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##EmitterBoardBlankPopup"))
        {
            string blankPasteReason{};
            const bool canPasteEmitterToEnd = Can_PasteEmitterToEnd(&blankPasteReason);

            if (ImGui::Selectable("새 이미터"))
            {
                Execute_AuthoringEdit(
                    "Insert Emitter",
                    [this]
                    {
                        Insert_NewEmitter(_emitters.size());
                    }
                );
            }

            if (!canPasteEmitterToEnd)
                ImGui::BeginDisabled();
            if (ImGui::MenuItem("이미터 붙여넣기"))
                Queue_ClipboardAction(PendingClipboardAction::PasteEmitterToEnd, _emitters.size());
            if (!canPasteEmitterToEnd)
                ImGui::EndDisabled();
            if (!canPasteEmitterToEnd && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !blankPasteReason.empty())
                ImGui::SetTooltip("%s", blankPasteReason.c_str());

            ImGui::Separator();

            if (ImGui::MenuItem("모든 이미터 활성화"))
                Set_AllEmittersEnabled(true);

            if (ImGui::MenuItem("모든 이미터 비활성화"))
                Set_AllEmittersEnabled(false);

            ImGui::Separator();

            const bool soloPreviewActive = Is_SoloPreviewActive();
            if (!soloPreviewActive)
                ImGui::BeginDisabled();
            if (ImGui::MenuItem("Solo 해제") && soloPreviewActive)
            {
                Clear_SoloPreviewEmitter();
                if (EDITOR != nullptr)
                    EDITOR->Request_RestartPreview();
            }
            if (!soloPreviewActive)
                ImGui::EndDisabled();

            ImGui::EndPopup();
        }

        Process_PendingEmitterAction();
        Process_PendingTypeDataAction();
        Process_PendingModuleAction();
        Process_PendingClipboardAction();
    }
    ImGui::EndChild();
}

void Emitter_View::Draw_EmitterColumn(size_t emitterIndex, AuthoringEmitter& emitter, float columnHeight)
{
    constexpr float headerPaddingX = 10.f;
    constexpr float headerPaddingY = 12.f;

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 headerColor = isSelected ? accentColor : To_ImVec4(kEmitterHeaderColor);
    const ImVec4 textColor = isSelected ? To_ImVec4(kEmitterSelectedTextColor) : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    ImGui::BeginGroup();
    const ImVec2 columnMin = ImGui::GetCursorScreenPos();
    const ImVec2 columnMax{
        columnMin.x + kEmitterColumnWidth,
        columnMin.y + columnHeight
    };
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        columnMin,
        columnMax,
        ImGui::ColorConvertFloat4ToU32(To_ImVec4(kEmitterColumnBackgroundColor))
    );
    drawList->AddRectFilled(
        ImVec2(columnMax.x - kEmitterDividerWidth, columnMin.y),
        columnMax,
        ImGui::ColorConvertFloat4ToU32(To_ImVec4(kEmitterDividerColor))
    );

    const ImVec2 headerCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, headerColor);
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::Button("##EmitterHeader", ImVec2(kEmitterColumnWidth, kEmitterHeaderHeight)))
        Select_Emitter(emitterIndex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_Emitter(emitterIndex);
    if (ImGui::BeginPopupContextItem("##EmitterHeaderPopup"))
    {
        Draw_EmitterContextMenu(emitterIndex);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);

    const ImVec2 moduleStackCursor = ImVec2(headerCursor.x, headerCursor.y + kEmitterHeaderHeight);
    ImGui::SetCursorPos(ImVec2(headerCursor.x + headerPaddingX, headerCursor.y + headerPaddingY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushTextWrapPos(headerCursor.x + kEmitterColumnWidth - headerPaddingX);
    ImGui::TextUnformatted(emitter.name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(
        ImVec2(
            headerCursor.x + headerPaddingX,
            headerCursor.y + kEmitterHeaderHeight - ImGui::GetFrameHeight() - headerPaddingY
        )
    );
    const float badgeSize = ImGui::GetFrameHeight();
    ImGui::PushID("EmitterEnabled");
    bool nextEmitterEnabled = emitter.enabled;
    if (ImGui::Checkbox("##Enabled", &nextEmitterEnabled))
    {
        const AuthoringSnapshot beforeEmitterEnabledSnapshot = Capture_AuthoringSnapshot();
        emitter.enabled = nextEmitterEnabled;
        emitter.previewDirty = true;
        const bool disabledSoloPreviewTarget = !emitter.enabled && Is_SoloPreviewEmitter(emitter.id);
        if (disabledSoloPreviewTarget)
            Clear_SoloPreviewEmitter();
        MarkDirty();
        const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
        Execute_AuthoringSnapshotCommand(beforeEmitterEnabledSnapshot, afterSnapshot, "Toggle Emitter Enabled");
        if (disabledSoloPreviewTarget && EDITOR != nullptr)
            EDITOR->Request_RestartPreview();
    }
    ImGui::PopID();

    ImGui::SameLine(0.f, 4.f);
    ImGui::PushID("SoloPreview");
    const bool canUseSoloPreview = emitter.enabled;
    const bool soloPreviewEnabled = Is_SoloPreviewEmitter(emitter.id);
    if (soloPreviewEnabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.82f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.50f, 0.92f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.36f, 0.72f, 1.f));
    }
    if (!canUseSoloPreview)
        ImGui::BeginDisabled();
    if (ImGui::Button("##SoloPreviewButton", ImVec2(badgeSize, badgeSize)) && canUseSoloPreview)
    {
        Select_Emitter(emitterIndex);
        Toggle_SoloPreviewEmitter(emitter.id);
    }
    const ImVec2 soloBadgeMin = ImGui::GetItemRectMin();
    const ImVec2 soloBadgeMax = ImGui::GetItemRectMax();
    const ImVec2 soloLabelSize = ImGui::CalcTextSize(kSoloPreviewIcon);
    const ImVec2 soloLabelPos{
        soloBadgeMin.x + (soloBadgeMax.x - soloBadgeMin.x - soloLabelSize.x) * 0.5f,
        soloBadgeMin.y + (soloBadgeMax.y - soloBadgeMin.y - soloLabelSize.y) * 0.5f
    };
    const ImVec4 soloLabelColor =
        canUseSoloPreview
        ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
        : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImGui::GetWindowDrawList()->AddText(
        soloLabelPos,
        ImGui::ColorConvertFloat4ToU32(soloLabelColor),
        kSoloPreviewIcon
    );
    if (!canUseSoloPreview)
        ImGui::EndDisabled();
    if (soloPreviewEnabled)
        ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered(canUseSoloPreview ? ImGuiHoveredFlags_None : ImGuiHoveredFlags_AllowWhenDisabled))
    {
        string tooltip{};
        if (!canUseSoloPreview)
            tooltip = "Emitter is disabled. Enable it before solo preview.";
        else
        {
            tooltip = soloPreviewEnabled ? "Solo preview enabled.\nOnly this emitter is sent to preview." : "Solo preview";
            if (Uses_ParticleEmitterSource(emitter))
                tooltip += "\nStrict solo excludes ParticleEmitter sources, so this emitter may preview empty or differently.";
        }
        ImGui::SetTooltip("%s", tooltip.c_str());
    }
    ImGui::PopID();

    ImGui::SameLine(0.f, 4.f);
    ImGui::PushID("PreviewTransform");
    const bool canUsePreviewTransform = EffectEditorInstance::Can_UsePreviewEmitterTransform(emitter);
    const bool previewTransformEnabled = EDITOR != nullptr && EDITOR->Is_PreviewEmitterTransformEnabled(emitter.id);
    if (previewTransformEnabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.82f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.50f, 0.92f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.36f, 0.72f, 1.f));
    }
    if (!canUsePreviewTransform)
        ImGui::BeginDisabled();
    if (ImGui::Button("##PreviewTransformButton", ImVec2(badgeSize, badgeSize)) && canUsePreviewTransform && EDITOR != nullptr)
    {
        Select_Emitter(emitterIndex);
        EDITOR->Set_PreviewEmitterTransformEnabled(emitter.id, !previewTransformEnabled);
    }
    const ImVec2 badgeMin = ImGui::GetItemRectMin();
    const ImVec2 badgeMax = ImGui::GetItemRectMax();
    const ImVec2 iconSize = ImGui::CalcTextSize(kPreviewTransformIcon);
    const ImVec2 iconPos{
        badgeMin.x + (badgeMax.x - badgeMin.x - iconSize.x) * 0.5f,
        badgeMin.y + (badgeMax.y - badgeMin.y - iconSize.y) * 0.5f
    };
    const ImVec4 iconColor =
        canUsePreviewTransform
        ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
        : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImGui::GetWindowDrawList()->AddText(
        iconPos,
        ImGui::ColorConvertFloat4ToU32(iconColor),
        kPreviewTransformIcon
    );
    if (!canUsePreviewTransform)
        ImGui::EndDisabled();
    if (previewTransformEnabled)
        ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered(canUsePreviewTransform ? ImGuiHoveredFlags_None : ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", Build_PreviewTransformTooltip(emitter, canUsePreviewTransform));
    ImGui::PopID();

    ImGui::SetCursorPos(moduleStackCursor);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.f));
    Draw_TypeDataRow(emitterIndex);
    for (size_t moduleIndex = 0; moduleIndex < emitter.modules.size(); ++moduleIndex)
    {
        ImGui::PushID(static_cast<int>(moduleIndex));
        Draw_ModuleRow(emitterIndex, moduleIndex, emitter.modules[moduleIndex]);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    const float reservedTailHeight = columnMax.y - ImGui::GetCursorScreenPos().y;
    if (reservedTailHeight > 0.f)
    {
        ImGui::InvisibleButton("##EmitterReservedTail", ImVec2(kEmitterColumnWidth, reservedTailHeight));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            Select_Emitter(emitterIndex);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            Select_Emitter(emitterIndex);
        if (ImGui::BeginPopupContextItem("##EmitterReservedTailPopup"))
        {
            Draw_EmitterContextMenu(emitterIndex);
            ImGui::EndPopup();
        }
    }

    ImGui::EndGroup();
}

void Emitter_View::Draw_TypeDataRow(size_t emitterIndex)
{
    constexpr float rowPaddingX = 10.f;

    const AuthoringTypeDataKind typeDataKind = _emitters[emitterIndex].typeData.kind;
    const bool hasTypeData = typeDataKind != AuthoringTypeDataKind::None;
    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedTypeData;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 rowColor = isSelected ? accentColor : To_ImVec4(kEmitterTypeDataRowColor);
    const ImVec4 textColor = isSelected
                             ? To_ImVec4(kEmitterSelectedTextColor)
                             : hasTypeData
                               ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                               : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rowColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::SetNextItemAllowOverlap();
    ImGui::Button("##TypeDataRow", ImVec2(kEmitterColumnWidth, kEmitterTypeDataRowHeight));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_TypeData(emitterIndex);
    ImGui::PopStyleColor(4);

    if (ImGui::BeginPopupContextItem("##TypeDataRowPopup"))
    {
        Draw_TypeDataContextMenu(emitterIndex);
        ImGui::EndPopup();
    }

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (kEmitterTypeDataRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (hasTypeData)
    {
        switch (typeDataKind)
        {
        case AuthoringTypeDataKind::Trail:
            ImGui::TextUnformatted("트레일 데이터");
            break;

        case AuthoringTypeDataKind::Mesh:
            ImGui::TextUnformatted("메시 데이터");
            break;

        case AuthoringTypeDataKind::Ribbon:
            ImGui::TextUnformatted("리본 데이터");
            break;

        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
            ImGui::TextUnformatted("스프라이트 트레일 데이터");
            break;

        case AuthoringTypeDataKind::Beam:
            ImGui::TextUnformatted("빔 데이터");
            break;

        case AuthoringTypeDataKind::None:
        default:
            ImGui::TextUnformatted("타입 데이터");
            break;
        }
    }
    else
        ImGui::TextUnformatted("타입 데이터 (미장착)");
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(afterRowCursor);
}

void Emitter_View::Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, AuthoringModule& module)
{
    constexpr float rowHeight = 28.f;
    constexpr float rowPaddingX = 10.f;
    constexpr float moduleCheckboxRightPadding = 34.f;
    constexpr float curveButtonSize = 22.f;
    constexpr float curveButtonGap = 4.f;

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedModuleIndex.has_value() &&
        _selectedModuleIndex.value() == moduleIndex;
    const bool isRequiredRow = module.type == AuthoringModuleType::Required;
    const bool isSpawnRow = module.type == AuthoringModuleType::Spawn;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 categoryColor = To_ImVec4(
        isRequiredRow
        ? kEmitterRequiredRowColor
        : isSpawnRow
          ? kEmitterSpawnRowColor
          : kEmitterCommonModuleRowColor
    );
    const ImVec4 disabledColor = To_ImVec4(kEmitterModuleDisabledRowColor);
    const bool usesCategoryColor = isRequiredRow || isSpawnRow;
    const ImVec4 rowColor = isSelected ? accentColor : categoryColor;
    const ImVec4 textColor = isSelected
                             ? To_ImVec4(kEmitterSelectedTextColor)
                             : usesCategoryColor
                               ? To_ImVec4(kEmitterCategoryTextColor)
                               : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    AuthoringEmitter& emitter = _emitters[emitterIndex];
    const bool moduleCompatible = Is_ModuleCompatibleWithEmitter(emitter, module.type);
    const bool canPinCurve = CurveEditor_View::Has_ModuleCurveTargets(emitter, module);
    const bool canConvertCurve = !canPinCurve && CurveEditor_View::Has_ModuleConvertibleCurveTargets(emitter, module);
    const bool canUseCurveButton = canPinCurve || canConvertCurve;
    bool isCurveEditorFocused = false;
    if (canPinCurve && EDITOR != nullptr)
    {
        if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
            isCurveEditorFocused = curveEditor->Is_ModuleFocused(emitter.id, module.id);
    }
    const string displayNameUtf8 = String::ToString(module.displayName);
    const ImVec4 moduleNameTextColor =
        !isSelected && module.enabled && !moduleCompatible
        ? To_ImVec4(kEmitterUnsupportedModuleTextColor)
        : module.enabled
          ? textColor
          : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    const ImVec4 buttonColor = module.enabled || usesCategoryColor ? rowColor : disabledColor;
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushID(module.key.c_str());
    ImGui::SetNextItemAllowOverlap();
    ImGui::Button("##ModuleRow", ImVec2(kEmitterColumnWidth, rowHeight));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_Module(emitterIndex, moduleIndex);

    const bool isRowHovered = ImGui::IsItemHovered();
    if (isCurveEditorFocused)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        drawList->AddRectFilled(
            rowMin,
            ImVec2{ rowMin.x + 6.f, rowMax.y },
            kCurveEditorFocusBarColor
        );
    }
    ImGui::PopStyleColor(4);
    if (ImGui::BeginPopupContextItem("##ModuleRowPopup"))
    {
        Draw_ModuleContextMenu(emitterIndex, moduleIndex, module);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, moduleNameTextColor);
    ImGui::TextUnformatted(displayNameUtf8.c_str());
    ImGui::PopStyleColor();

    if (!isRequiredRow)
    {
        const float curveButtonRightPadding = moduleCheckboxRightPadding + curveButtonSize + curveButtonGap;
        ImGui::SetCursorPos(
            ImVec2(
                rowCursor.x + kEmitterColumnWidth - curveButtonRightPadding,
                rowCursor.y + (rowHeight - curveButtonSize) * 0.5f
            )
        );
        ImGui::PushID("PinCurves");
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.42f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (!canUseCurveButton)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_CHART_LINE, ImVec2(curveButtonSize, curveButtonSize)) && canUseCurveButton)
        {
            if (canPinCurve && EDITOR != nullptr)
            {
                if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
                    curveEditor->Pin_ModuleTargets(emitter.id, module.id);
            }
            else if (canConvertCurve)
                ImGui::OpenPopup("ConvertCurvesPopup");
        }
        if (!canUseCurveButton)
            ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(canUseCurveButton ? ImGuiHoveredFlags_None : ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(
                canPinCurve
                ? "커브 편집"
                : canConvertCurve
                  ? "커브로 전환 후 편집"
                  : "이 모듈에서는 커브 분포가 지원되지 않습니다"
            );
        }
        if (ImGui::BeginPopup("ConvertCurvesPopup"))
        {
            ImGui::TextUnformatted("이 모듈의 분포를 고정값 커브로 전환할까요?");
            ImGui::TextDisabled("전환 후 Curve Editor에서 바로 편집합니다.");
            if (ImGui::Button("전환", ImVec2{ 72.f, 0.f }))
            {
                if (EDITOR != nullptr)
                {
                    if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
                        curveEditor->Convert_AndPin_ModuleTargets(emitter.id, module.id);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("취소", ImVec2{ 72.f, 0.f }))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopID();

        ImGui::SetCursorPos(
            ImVec2(
                rowCursor.x + kEmitterColumnWidth - moduleCheckboxRightPadding,
                rowCursor.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f
            )
        );
        ImGui::PushID("ModuleEnabled");
        bool nextModuleEnabled = module.enabled;
        if (ImGui::Checkbox("##Enabled", &nextModuleEnabled))
        {
            const AuthoringSnapshot beforeModuleEnabledSnapshot = Capture_AuthoringSnapshot();
            module.enabled = nextModuleEnabled;
            emitter.previewDirty = true;
            MarkDirty();
            const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
            Execute_AuthoringSnapshotCommand(beforeModuleEnabledSnapshot, afterSnapshot, "Toggle Module Enabled");
        }
        ImGui::PopID();
    }

    ImGui::SetCursorPos(afterRowCursor);

    if (isRowHovered)
    {
        const bool warningTooltipNeeded =
            !emitter.enabled ||
            (!module.enabled && module.type != AuthoringModuleType::Required) ||
            !moduleCompatible;
        if (warningTooltipNeeded)
        {
            const string warningTooltip = Build_ModuleWarningTooltip(emitter, module);
            if (!warningTooltip.empty())
                ImGui::SetTooltip("%s", warningTooltip.c_str());
        }
    }
}
NS_END
