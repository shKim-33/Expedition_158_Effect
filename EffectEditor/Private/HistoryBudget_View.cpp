#include "HistoryBudget_View.h"

#include "Emitter_View.h"
#include "EffectEditorInstance.h"
#include "EffectEditorPreviewDefinitionBuilder.h"
#include "GameInstance.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr float kHighlightDuration{ 1.5f };

    const char* Get_PresetLabel(HistoryBudgetPreset value)
    {
        switch (value)
        {
        case HistoryBudgetPreset::High:
            return "High";
        case HistoryBudgetPreset::Medium:
            return "Medium";
        case HistoryBudgetPreset::Performance:
            return "Performance";
        case HistoryBudgetPreset::Low:
            return "Low";
        case HistoryBudgetPreset::Full:
        default:
            return "Full";
        }
    }

    const char* Get_PriorityLabel(HistoryBudgetPriority value)
    {
        switch (value)
        {
        case HistoryBudgetPriority::Secondary:
            return "Secondary";
        case HistoryBudgetPriority::Decorative:
            return "Decorative";
        case HistoryBudgetPriority::Distortion:
            return "Distortion";
        case HistoryBudgetPriority::Core:
        default:
            return "Core";
        }
    }

    const char* Get_DensityBiasLabel(HistoryBudgetDensityBias value)
    {
        switch (value)
        {
        case HistoryBudgetDensityBias::Dense:
            return "Dense";
        case HistoryBudgetDensityBias::Sparse:
            return "Sparse";
        case HistoryBudgetDensityBias::Normal:
        default:
            return "Normal";
        }
    }

    const char* Get_TypeLabel(AuthoringTypeDataKind value)
    {
        switch (value)
        {
        case AuthoringTypeDataKind::Trail:
            return "Trail";
        case AuthoringTypeDataKind::Ribbon:
            return "Ribbon";
        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
            return "SpriteTrail";
        case AuthoringTypeDataKind::Mesh:
            return "Mesh";
        case AuthoringTypeDataKind::Beam:
            return "Beam";
        case AuthoringTypeDataKind::None:
        default:
            return "Sprite";
        }
    }

    bool Is_HistoryFamily(AuthoringTypeDataKind value)
    {
        return value == AuthoringTypeDataKind::Trail ||
               value == AuthoringTypeDataKind::Ribbon ||
               value == AuthoringTypeDataKind::SourceHistorySpriteTrail;
    }

    const RequiredModuleData* Find_RequiredModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            return get_if<RequiredModuleData>(&module.data);
        }

        return nullptr;
    }

    bool Has_DistortionSignal(const AuthoringEmitter& emitter)
    {
        if (emitter.historyBudget.priority == HistoryBudgetPriority::Distortion)
            return true;

        const RequiredModuleData* required = Find_RequiredModule(emitter);
        return required != nullptr &&
               required->material.materialFamily == EffectMaterialFamily::SpriteDistortion;
    }

    string Format_Counts(const EffectHistoryBudgetEffectiveDesc& data)
    {
        return format("{} / {} / {}", data.sourceSampleCount, data.renderSegmentCount, data.spriteStampCount);
    }

    const EffectEmitterDefinition* Find_Definition(const Shared<const EffectDefinition>& definition, uint32 emitterId)
    {
        if (definition == nullptr)
            return nullptr;

        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (emitter.id == emitterId)
                return &emitter;
        }

        return nullptr;
    }

    EffectHistoryBudgetEffectiveDesc Compute_TargetEffective(
        const EffectDefinition& budgetDefinition,
        const EffectEmitterDefinition& fullEmitter)
    {
        return Compute_EffectHistoryBudgetEffective(
            budgetDefinition.historyBudget,
            fullEmitter.historyBudget,
            EffectHistoryBudgetEffectiveInput{
                fullEmitter.effectiveHistoryBudget.family,
                fullEmitter.effectiveHistoryBudget.sourceSampleCount,
                fullEmitter.effectiveHistoryBudget.renderSegmentCount,
                fullEmitter.effectiveHistoryBudget.spriteStampCount
            }
        );
    }

    string Build_WarningText(
        const AuthoringEmitter& emitter,
        const EffectHistoryBudgetEffectiveDesc* target,
        const EffectHistoryBudgetEffectiveDesc* actual,
        uint32 historyFamilyCount)
    {
        vector<string> warnings{};
        if (target != nullptr && actual != nullptr)
        {
            if (target->sourceSampleCount < actual->sourceSampleCount)
                warnings.push_back("소스 보존");
            if (target->renderSegmentCount < actual->renderSegmentCount ||
                target->spriteStampCount < actual->spriteStampCount)
            {
                warnings.push_back("작성값 하한");
            }
        }

        if (Is_HistoryFamily(emitter.typeData.kind) && historyFamilyCount > 1u)
            warnings.push_back("공유 대상");

        if (Has_DistortionSignal(emitter))
            warnings.push_back("왜곡 보정 보류");

        if (warnings.empty())
            return "-";

        string text = warnings.front();
        for (size_t index = 1; index < warnings.size(); ++index)
            text += ", " + warnings[index];
        return text;
    }

    const char* Get_SourceGroupKindLabel(EffectHistorySourceGroupKind value)
    {
        switch (value)
        {
        case EffectHistorySourceGroupKind::TrailPairHistory:
            return "TrailPairHistory";
        case EffectHistorySourceGroupKind::SourcePointHistory:
            return "SourcePointHistory";
        case EffectHistorySourceGroupKind::None:
        default:
            return "None";
        }
    }

    string Build_SourceGroupId(const AuthoringEmitter& emitter)
    {
        switch (emitter.typeData.kind)
        {
        case AuthoringTypeDataKind::Trail:
            return "TrailPairHistory|TrailProvider";
        case AuthoringTypeDataKind::Ribbon:
        {
            const RibbonTypeData* data = get_if<RibbonTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                return {};

            if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                return format("SourcePointHistory|ParticleEmitter:{}", data->sourceEmitterId);

            return "SourcePointHistory|SelfRoot";
        }
        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
        {
            const SourceHistorySpriteTrailTypeData* data = get_if<SourceHistorySpriteTrailTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                return {};

            if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                return format("SourcePointHistory|ParticleEmitter:{}", data->sourceEmitterId);

            return "SourcePointHistory|SelfRoot";
        }
        default:
            return {};
        }
    }

    EffectHistorySourceGroupKind Resolve_SourceGroupKind(const string& groupId)
    {
        if (groupId.starts_with("TrailPairHistory|"))
            return EffectHistorySourceGroupKind::TrailPairHistory;
        if (groupId.starts_with("SourcePointHistory|"))
            return EffectHistorySourceGroupKind::SourcePointHistory;
        return EffectHistorySourceGroupKind::None;
    }

    string Build_SourceGroupLabel(const string& groupId)
    {
        const size_t separator = groupId.find('|');
        if (separator == string::npos || separator + 1u >= groupId.size())
            return groupId;

        return groupId.substr(separator + 1u);
    }

    HistorySourceGroupBudgetData Build_SourceGroupBudgetCandidate(const AuthoringEmitter& emitter)
    {
        HistorySourceGroupBudgetData candidate{};
        candidate.confirmed = false;

        switch (emitter.typeData.kind)
        {
        case AuthoringTypeDataKind::Trail:
        {
            const TrailTypeData* data = get_if<TrailTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                break;

            candidate.kind = EffectHistorySourceGroupKind::TrailPairHistory;
            candidate.stableId = "TrailProvider";
            candidate.sourceSampleCount = max(2u, data->historyCount);
            candidate.sampleSpacing = max(0.001f, data->sampleSpacing);
            candidate.curveSubdivision = data->curveSubdivision;
            candidate.smoothTangent = data->smoothTangent;
            break;
        }
        case AuthoringTypeDataKind::Ribbon:
        {
            const RibbonTypeData* data = get_if<RibbonTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                break;

            candidate.kind = EffectHistorySourceGroupKind::SourcePointHistory;
            candidate.stableId = data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter
                                 ? format("ParticleEmitter:{}", data->sourceEmitterId)
                                 : "SelfRoot";
            candidate.sourceSampleCount = max(2u, data->maxSampleCount);
            candidate.sampleSpacing = max(0.001f, data->sampleSpacing);
            candidate.sampleInterval = max(0.f, data->sampleInterval);
            candidate.curveSubdivision = data->curveSubdivision;
            candidate.smoothTangent = data->smoothTangent;
            break;
        }
        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
        {
            const SourceHistorySpriteTrailTypeData* data = get_if<SourceHistorySpriteTrailTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                break;

            candidate.kind = EffectHistorySourceGroupKind::SourcePointHistory;
            candidate.stableId = data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter
                                 ? format("ParticleEmitter:{}", data->sourceEmitterId)
                                 : "SelfRoot";
            candidate.sourceSampleCount =
                max(2u, static_cast<uint32>(ceilf(max(0.0001f, data->sampleLifetime) / max(0.001f, data->sampleSpacing))) + 2u);
            candidate.sampleLifetime = max(0.0001f, data->sampleLifetime);
            candidate.sampleSpacing = max(0.001f, data->sampleSpacing);
            candidate.curveSubdivision = data->curveSubdivision;
            candidate.smoothTangent = data->smoothTangent;
            break;
        }
        default:
            break;
        }

        return candidate;
    }

    void Merge_SourceGroupBudgetCandidate(HistorySourceGroupBudgetData& target, const HistorySourceGroupBudgetData& source)
    {
        if (target.kind == EffectHistorySourceGroupKind::None)
        {
            target = source;
            return;
        }

        target.sourceSampleCount = max(target.sourceSampleCount, source.sourceSampleCount);
        if (source.sampleLifetime > 0.f)
            target.sampleLifetime = max(target.sampleLifetime, source.sampleLifetime);
        if (source.sampleSpacing > 0.f)
            target.sampleSpacing = target.sampleSpacing > 0.f ? min(target.sampleSpacing, source.sampleSpacing) : source.sampleSpacing;
        target.sampleInterval = max(target.sampleInterval, source.sampleInterval);
        target.curveSubdivision = max(target.curveSubdivision, source.curveSubdivision);
        target.smoothTangent = target.smoothTangent || source.smoothTangent;
    }

    const HistorySourceGroupBudgetData* Find_SourceGroupBudget(const HistoryBudgetData& budget, const string& groupId)
    {
        const size_t separator = groupId.find('|');
        if (separator == string::npos || separator + 1u >= groupId.size())
            return nullptr;

        const EffectHistorySourceGroupKind kind = Resolve_SourceGroupKind(groupId);
        const string stableId = groupId.substr(separator + 1u);
        for (const HistorySourceGroupBudgetData& group : budget.sourceGroups)
        {
            if (group.kind == kind && group.stableId == stableId)
                return &group;
        }

        return nullptr;
    }

    void Upsert_SourceGroupBudget(HistoryBudgetData& budget, HistorySourceGroupBudgetData group)
    {
        group.confirmed = true;
        for (HistorySourceGroupBudgetData& existing : budget.sourceGroups)
        {
            if (existing.kind == group.kind && existing.stableId == group.stableId)
            {
                existing = group;
                return;
            }
        }

        budget.sourceGroups.push_back(group);
    }

    bool Nearly_Equal(float lhs, float rhs)
    {
        return fabsf(lhs - rhs) <= 0.0001f;
    }

    bool Is_SameBudgetValues(const HistorySourceGroupBudgetData& lhs, const HistorySourceGroupBudgetData& rhs)
    {
        return lhs.kind == rhs.kind &&
               lhs.stableId == rhs.stableId &&
               lhs.sourceSampleCount == rhs.sourceSampleCount &&
               Nearly_Equal(lhs.sampleLifetime, rhs.sampleLifetime) &&
               Nearly_Equal(lhs.sampleSpacing, rhs.sampleSpacing) &&
               Nearly_Equal(lhs.sampleInterval, rhs.sampleInterval) &&
               lhs.curveSubdivision == rhs.curveSubdivision &&
               lhs.smoothTangent == rhs.smoothTangent;
    }

    struct EmitterBudgetRow
    {
        const AuthoringEmitter* emitter{};
        const EffectEmitterDefinition* fullEmitter{};
        const EffectEmitterDefinition* actualEmitter{};
        optional<EffectHistoryBudgetEffectiveDesc> targetEffective{};
        string groupId{};
    };

    struct SourceGroupRow
    {
        string id{};
        string label{};
        EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
        vector<size_t> rowIndices{};
        uint32 currentRequestCount{};
        uint32 sharedRequestCount{};
        uint32 savedRequestCount{};
        uint32 currentSourceSamples{};
        uint32 sharedSourceSamples{};
        uint32 savedSourceSamples{};
        bool hasRibbon{};
        bool hasSpriteTrail{};
        bool hasDistortion{};
        HistorySourceGroupBudgetData candidateBudget{};
        const HistorySourceGroupBudgetData* savedBudget{};
    };

    vector<EmitterBudgetRow> Build_EmitterBudgetRows(
        const vector<AuthoringEmitter>& emitters,
        const Shared<const EffectDefinition>& fullDefinition,
        const Shared<const EffectDefinition>& actualDefinition)
    {
        vector<EmitterBudgetRow> rows{};
        rows.reserve(emitters.size());

        for (const AuthoringEmitter& emitter : emitters)
        {
            if (!emitter.enabled || !Is_HistoryFamily(emitter.typeData.kind))
                continue;

            const string groupId = Build_SourceGroupId(emitter);
            if (groupId.empty())
                continue;

            const EffectEmitterDefinition* fullEmitter = Find_Definition(fullDefinition, emitter.id);
            const EffectEmitterDefinition* actualEmitter = Find_Definition(actualDefinition, emitter.id);

            optional<EffectHistoryBudgetEffectiveDesc> targetEffective{};
            if (fullEmitter != nullptr && actualDefinition != nullptr)
                targetEffective = Compute_TargetEffective(*actualDefinition, *fullEmitter);

            rows.push_back(
                EmitterBudgetRow{
                    .emitter = &emitter,
                    .fullEmitter = fullEmitter,
                    .actualEmitter = actualEmitter,
                    .targetEffective = targetEffective,
                    .groupId = groupId
                }
            );
        }

        return rows;
    }

    vector<SourceGroupRow> Build_SourceGroupRows(const vector<EmitterBudgetRow>& emitterRows, const HistoryBudgetData& budget)
    {
        vector<SourceGroupRow> groups{};

        for (size_t rowIndex = 0; rowIndex < emitterRows.size(); ++rowIndex)
        {
            const EmitterBudgetRow& emitterRow = emitterRows[rowIndex];
            const AuthoringEmitter* emitter = emitterRow.emitter;
            if (emitter == nullptr)
                continue;

            auto groupIter = ranges::find_if(
                groups,
                [&emitterRow](const SourceGroupRow& group)
                {
                    return group.id == emitterRow.groupId;
                }
            );
            if (groupIter == groups.end())
            {
                SourceGroupRow group{};
                group.id = emitterRow.groupId;
                group.label = Build_SourceGroupLabel(emitterRow.groupId);
                group.kind = Resolve_SourceGroupKind(emitterRow.groupId);
                groups.push_back(group);
                groupIter = prev(groups.end());
            }

            SourceGroupRow& group = *groupIter;
            group.rowIndices.push_back(rowIndex);
            group.currentRequestCount++;
            group.sharedRequestCount = 1u;
            group.savedBudget = Find_SourceGroupBudget(budget, group.id);
            Merge_SourceGroupBudgetCandidate(group.candidateBudget, Build_SourceGroupBudgetCandidate(*emitter));

            const EffectHistoryBudgetEffectiveDesc* actual =
                emitterRow.actualEmitter != nullptr ? &emitterRow.actualEmitter->effectiveHistoryBudget : nullptr;
            if (actual != nullptr)
            {
                group.currentSourceSamples += actual->sourceSampleCount;
                group.sharedSourceSamples = max(group.sharedSourceSamples, actual->sourceSampleCount);
            }

            group.hasRibbon = group.hasRibbon || emitter->typeData.kind == AuthoringTypeDataKind::Ribbon;
            group.hasSpriteTrail = group.hasSpriteTrail || emitter->typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
            group.hasDistortion = group.hasDistortion || Has_DistortionSignal(*emitter);
        }

        for (SourceGroupRow& group : groups)
        {
            group.savedRequestCount = group.currentRequestCount > group.sharedRequestCount
                                      ? group.currentRequestCount - group.sharedRequestCount
                                      : 0u;
            group.savedSourceSamples = group.currentSourceSamples > group.sharedSourceSamples
                                       ? group.currentSourceSamples - group.sharedSourceSamples
                                       : 0u;
        }

        return groups;
    }

    string Build_SourceGroupStatusText(const SourceGroupRow& group)
    {
        vector<string> statuses{};
        if (group.savedBudget == nullptr)
            statuses.push_back("미확정");
        else if (!group.savedBudget->confirmed)
            statuses.push_back("미확정 저장값");
        else if (!Is_SameBudgetValues(*group.savedBudget, group.candidateBudget))
            statuses.push_back("불일치");
        else
            statuses.push_back("확정됨");

        if (group.currentRequestCount <= 1u)
            statuses.push_back("단일 이미터: 절감 없음");
        else
            statuses.push_back("소스 공유 활성");

        if (group.hasRibbon && group.hasSpriteTrail)
            statuses.push_back("리본/스프라이트 공용 소스");
        if (group.hasDistortion)
            statuses.push_back("왜곡 보정 보류");

        string text = statuses.front();
        for (size_t index = 1; index < statuses.size(); ++index)
            text += ", " + statuses[index];
        return text;
    }

    const SourceGroupRow* Find_SourceGroup(const vector<SourceGroupRow>& groups, const string& groupId)
    {
        const auto iter = ranges::find_if(
            groups,
            [&groupId](const SourceGroupRow& group)
            {
                return group.id == groupId;
            }
        );

        return iter != groups.end() ? &*iter : nullptr;
    }

    bool Draw_PresetCombo(HistoryBudgetPreset& value)
    {
        bool changed = false;
        constexpr HistoryBudgetPreset kValues[]{
            HistoryBudgetPreset::Full,
            HistoryBudgetPreset::High,
            HistoryBudgetPreset::Medium,
            HistoryBudgetPreset::Performance,
            HistoryBudgetPreset::Low
        };

        if (ImGui::BeginCombo("프리셋", Get_PresetLabel(value)))
        {
            for (const HistoryBudgetPreset candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_PresetLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        return changed;
    }

}

HistoryBudget_View::HistoryBudget_View()
    : Editor_Window{ L"History Budget", ICON_FA_CHART_SIMPLE }
{
    Set_Open(false);
}

void HistoryBudget_View::Update(float timeDelta)
{
    if (_highlightTimer > 0.f)
        _highlightTimer = max(0.f, _highlightTimer - max(0.f, timeDelta));

    if (_highlightTimer <= 0.f)
        _highlightEmitterId.reset();
}

void HistoryBudget_View::Render()
{
    bool isOpen = Is_Open();
    if (!isOpen)
        return;

    ImGui::SetNextWindowSize(ImVec2{ 900.f, 520.f }, ImGuiCond_FirstUseEver);
    Apply_PendingFocusBeforeBegin();
    if (ImGui::Begin(Get_ImGuiWindowName().c_str(), &isOpen))
    {
        Clear_PendingFocusAfterBegin();
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        const Shared<Emitter_View> emitterView = Get_EmitterView();
        if (emitterView == nullptr)
        {
            ImGui::TextDisabled("Emitter view is unavailable.");
        }
        else
        {
            Draw_SourceGroupSummary(*emitterView);
        }
    }
    ImGui::End();

    Set_Open(isOpen);
}

void HistoryBudget_View::Open_Target(uint32 emitterId)
{
    Set_Open(true);
    Request_FocusOnOpen();
    _pendingFocusEmitterId = emitterId;
    _highlightEmitterId = emitterId;
    _highlightTimer = kHighlightDuration;
}

Shared<Emitter_View> HistoryBudget_View::Get_EmitterView() const
{
    if (EDITOR == nullptr)
        return nullptr;

    return dynamic_pointer_cast<Emitter_View>(EDITOR->Get_Window(L"Emitter"));
}

void HistoryBudget_View::Draw_SourceGroupSummary(Emitter_View& emitterView)
{
    const vector<AuthoringEmitter>& emitters = emitterView.Get_Emitters();
    uint32 historyFamilyCount = 0u;
    for (const AuthoringEmitter& emitter : emitters)
    {
        if (emitter.enabled && Is_HistoryFamily(emitter.typeData.kind))
            ++historyFamilyCount;
    }

    HistoryBudgetData fullBudget = emitterView.Get_HistoryBudget();
    fullBudget.preset = HistoryBudgetPreset::Full;

    const Shared<const EffectDefinition> fullDefinition =
        PreviewDefinition::Build_EffectDefinition(emitters, fullBudget, true);
    const Shared<const EffectDefinition> actualDefinition =
        PreviewDefinition::Build_EffectDefinition(emitters, emitterView.Get_HistoryBudget(), true);

    const vector<EmitterBudgetRow> emitterRows = Build_EmitterBudgetRows(emitters, fullDefinition, actualDefinition);
    const vector<SourceGroupRow> sourceGroups = Build_SourceGroupRows(emitterRows, emitterView.Get_HistoryBudget());

    if (_pendingFocusEmitterId.has_value())
    {
        for (const EmitterBudgetRow& row : emitterRows)
        {
            if (row.emitter != nullptr && row.emitter->id == _pendingFocusEmitterId.value())
            {
                _selectedSourceGroupId = row.groupId;
                break;
            }
        }
    }

    const bool selectedGroupValid = ranges::any_of(
        sourceGroups,
        [this](const SourceGroupRow& group)
        {
            return group.id == _selectedSourceGroupId;
        }
    );
    if (!selectedGroupValid)
        _selectedSourceGroupId = sourceGroups.empty() ? string{} : sourceGroups.front().id;

    const SourceGroupRow* selectedGroup = Find_SourceGroup(sourceGroups, _selectedSourceGroupId);

    constexpr float controlPanelWidth = 320.f;
    ImGui::BeginChild("##HistoryBudgetControlPanel", ImVec2(controlPanelWidth, 0.f), true);
    {
        HistoryBudgetData nextBudget = emitterView.Get_HistoryBudget();
        const bool needsSharedPolicySave = !nextBudget.sharedSourceHistory;
        nextBudget.sharedSourceHistory = true;

        bool changed = false;
        ImGui::TextUnformatted("예산");
        ImGui::PushItemWidth(-1.f);
        changed |= Draw_PresetCombo(nextBudget.preset);
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("정책");
        ImGui::TextDisabled("소스 샘플 공유: 자동 적용");
        ImGui::TextDisabled("표시 길이: Detail 값 사용");
        if (selectedGroup != nullptr)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextUnformatted("선택 그룹");
            ImGui::TextWrapped(
                "%s / %s",
                Get_SourceGroupKindLabel(selectedGroup->kind),
                selectedGroup->label.c_str()
            );
            ImGui::TextDisabled(
                "이미터 %u개 -> 소스 %u개",
                selectedGroup->currentRequestCount,
                selectedGroup->sharedRequestCount
            );
            ImGui::TextDisabled(
                "절감 %u 호출 / %u src",
                selectedGroup->savedRequestCount,
                selectedGroup->savedSourceSamples
            );

            const bool hasSavedBudget = selectedGroup->savedBudget != nullptr;
            if (hasSavedBudget)
                ImGui::TextDisabled("저장 기준: %u samples / %u subdiv",
                    selectedGroup->savedBudget->sourceSampleCount,
                    selectedGroup->savedBudget->curveSubdivision);
            ImGui::TextDisabled("추천 기준: %u samples / %u subdiv",
                selectedGroup->candidateBudget.sourceSampleCount,
                selectedGroup->candidateBudget.curveSubdivision);

            if (ImGui::Button(hasSavedBudget ? "추천 기준 다시 채택" : "추천 기준 채택"))
            {
                Upsert_SourceGroupBudget(nextBudget, selectedGroup->candidateBudget);
                changed = true;
            }

            HistorySourceGroupBudgetData editableBudget =
                hasSavedBudget ? *selectedGroup->savedBudget : selectedGroup->candidateBudget;
            editableBudget.kind = selectedGroup->kind;
            editableBudget.stableId = Build_SourceGroupLabel(selectedGroup->id);
            editableBudget.confirmed = hasSavedBudget && selectedGroup->savedBudget->confirmed;

            ImGui::Spacing();
            ImGui::TextUnformatted("기준값 수정");
            bool groupChanged = false;
            groupChanged |= ImGui::DragScalar("소스 샘플", ImGuiDataType_U32, &editableBudget.sourceSampleCount, 1.f);
            groupChanged |= ImGui::DragFloat("샘플 간격", &editableBudget.sampleSpacing, 0.001f, 0.f, 10000.f, "%.3f");
            groupChanged |= ImGui::DragScalar("곡선 분할", ImGuiDataType_U32, &editableBudget.curveSubdivision, 1.f);
            groupChanged |= ImGui::Checkbox("탄젠트 스무딩", &editableBudget.smoothTangent);
            if (selectedGroup->kind == EffectHistorySourceGroupKind::SourcePointHistory)
            {
                groupChanged |= ImGui::DragFloat("샘플 수명", &editableBudget.sampleLifetime, 0.01f, 0.f, 10000.f, "%.3f");
                groupChanged |= ImGui::DragFloat("샘플 보강 간격", &editableBudget.sampleInterval, 0.001f, 0.f, 10000.f, "%.3f");
            }

            if (groupChanged)
            {
                editableBudget.sourceSampleCount = max(0u, editableBudget.sourceSampleCount);
                editableBudget.sampleLifetime = max(0.f, editableBudget.sampleLifetime);
                editableBudget.sampleSpacing = max(0.f, editableBudget.sampleSpacing);
                editableBudget.sampleInterval = max(0.f, editableBudget.sampleInterval);
                editableBudget.curveSubdivision = max(0u, editableBudget.curveSubdivision);
                Upsert_SourceGroupBudget(nextBudget, editableBudget);
                changed = true;
            }
        }
        else
        {
            ImGui::TextDisabled("히스토리 소스 그룹 없음");
        }

        if (changed || needsSharedPolicySave)
            emitterView.Set_HistoryBudget(nextBudget, "Edit History Budget");
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##HistoryBudgetDiagnostics", ImVec2(0.f, 0.f), false);
    ImGui::TextUnformatted("소스 그룹");
    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("HistoryBudgetSourceGroups", 6, tableFlags, ImVec2(0.f, 150.f)))
    {
        ImGui::TableSetupColumn("그룹", ImGuiTableColumnFlags_WidthFixed, 210.f);
        ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthFixed, 130.f);
        ImGui::TableSetupColumn("이미터", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("목표", ImGuiTableColumnFlags_WidthFixed, 130.f);
        ImGui::TableSetupColumn("절감", ImGuiTableColumnFlags_WidthFixed, 120.f);
        ImGui::TableSetupColumn("상태", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const SourceGroupRow& group : sourceGroups)
        {
            ImGui::TableNextRow();
            if (group.id == _selectedSourceGroupId)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(62, 92, 130, 115));

            ImGui::TableSetColumnIndex(0);
            const bool selected = group.id == _selectedSourceGroupId;
            if (ImGui::Selectable(group.label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                _selectedSourceGroupId = group.id;
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(Get_SourceGroupKindLabel(group.kind));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", static_cast<uint32>(group.rowIndices.size()));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u 호출 / %u src", group.sharedRequestCount, group.sharedSourceSamples);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u / %u", group.savedRequestCount, group.savedSourceSamples);
            ImGui::TableSetColumnIndex(5);
            const string statusText = Build_SourceGroupStatusText(group);
            ImGui::TextWrapped("%s", statusText.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("이미터 진단");
    if (ImGui::BeginTable("HistoryBudgetEmitterBreakdown", 8, tableFlags, ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupColumn("이미터", ImGuiTableColumnFlags_WidthFixed, 190.f);
        ImGui::TableSetupColumn("타입", ImGuiTableColumnFlags_WidthFixed, 86.f);
        ImGui::TableSetupColumn("우선도", ImGuiTableColumnFlags_WidthFixed, 86.f);
        ImGui::TableSetupColumn("Bias", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Full s/r/st", ImGuiTableColumnFlags_WidthFixed, 105.f);
        ImGui::TableSetupColumn("목표", ImGuiTableColumnFlags_WidthFixed, 105.f);
        ImGui::TableSetupColumn("실제", ImGuiTableColumnFlags_WidthFixed, 105.f);
        ImGui::TableSetupColumn("상태", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const EmitterBudgetRow& row : emitterRows)
        {
            if (row.groupId != _selectedSourceGroupId)
                continue;

            const AuthoringEmitter* emitter = row.emitter;
            if (emitter == nullptr)
                continue;

            const EffectHistoryBudgetEffectiveDesc* targetPtr =
                row.targetEffective.has_value() ? &row.targetEffective.value() : nullptr;
            const EffectHistoryBudgetEffectiveDesc* actualPtr =
                row.actualEmitter != nullptr ? &row.actualEmitter->effectiveHistoryBudget : nullptr;

            ImGui::TableNextRow();
            if (_highlightEmitterId.has_value() && _highlightEmitterId.value() == emitter->id)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(86, 116, 60, 130));

            if (_pendingFocusEmitterId.has_value() && _pendingFocusEmitterId.value() == emitter->id)
            {
                ImGui::SetScrollHereY(0.5f);
                _pendingFocusEmitterId.reset();
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(emitter->name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(Get_TypeLabel(emitter->typeData.kind));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(Get_PriorityLabel(emitter->historyBudget.priority));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(Get_DensityBiasLabel(emitter->historyBudget.densityBias));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(row.fullEmitter != nullptr ? Format_Counts(row.fullEmitter->effectiveHistoryBudget).c_str() : "-");
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(targetPtr != nullptr ? Format_Counts(*targetPtr).c_str() : "-");
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(actualPtr != nullptr ? Format_Counts(*actualPtr).c_str() : "-");
            ImGui::TableSetColumnIndex(7);
            const string warningText = Build_WarningText(*emitter, targetPtr, actualPtr, historyFamilyCount);
            ImGui::TextWrapped("%s", warningText.c_str());
        }

        ImGui::EndTable();
    }
    ImGui::EndChild();
}

Shared<HistoryBudget_View> HistoryBudget_View::Create()
{
    return make_shared<HistoryBudget_View>();
}

NS_END
