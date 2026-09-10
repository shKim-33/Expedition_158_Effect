#include "Detail_View.h"

#include "DetailPropertyContext.h"
#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "HistoryBudget_View.h"
#include "Level_EffectEditor.h"
#include "MeshDataPreview_View.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr TrailCurveQuality kTrailCurveQualityValues[] = {
        TrailCurveQuality::Basic,
        TrailCurveQuality::Smooth,
        TrailCurveQuality::HighQuality,
        TrailCurveQuality::Custom,
    };

    constexpr RibbonCurveQuality kRibbonCurveQualityValues[] = {
        RibbonCurveQuality::Basic,
        RibbonCurveQuality::Smooth,
        RibbonCurveQuality::HighQuality,
        RibbonCurveQuality::Custom,
    };

    const char* Get_TrailCurveQualityLabel(TrailCurveQuality value)
    {
        switch (value)
        {
        case TrailCurveQuality::Basic:
            return "기본";
        case TrailCurveQuality::Smooth:
            return "부드럽게";
        case TrailCurveQuality::HighQuality:
            return "고품질";
        case TrailCurveQuality::Custom:
            return "사용자 지정";
        default:
            return "기본";
        }
    }

    const char* Get_TrailCurveQualityTooltip(TrailCurveQuality value)
    {
        switch (value)
        {
        case TrailCurveQuality::Basic:
            return "기존 호환에 가까운 낮은 보간 비용 설정입니다.";
        case TrailCurveQuality::Smooth:
            return "샘플 간격과 보간 수를 늘려 swing arc의 각짐을 줄입니다.";
        case TrailCurveQuality::HighQuality:
            return "더 촘촘한 샘플로 곡선을 만들지만 preview 비용이 늘 수 있습니다.";
        case TrailCurveQuality::Custom:
            return "아래 세부 곡선 값을 직접 편집합니다.";
        default:
            return "";
        }
    }

    const char* Get_RibbonCurveQualityLabel(RibbonCurveQuality value)
    {
        switch (value)
        {
        case RibbonCurveQuality::Basic:
            return "기본";
        case RibbonCurveQuality::Smooth:
            return "부드럽게";
        case RibbonCurveQuality::HighQuality:
            return "고품질";
        case RibbonCurveQuality::Custom:
            return "사용자 지정";
        default:
            return "기본";
        }
    }

    const char* Get_RibbonCurveQualityTooltip(RibbonCurveQuality value)
    {
        switch (value)
        {
        case RibbonCurveQuality::Basic:
            return "낮은 비용의 기본 source history 곡선 sample 밀도입니다.";
        case RibbonCurveQuality::Smooth:
            return "샘플 간격과 보간 수를 늘려 source history path의 각짐을 줄입니다.";
        case RibbonCurveQuality::HighQuality:
            return "가장 촘촘한 sample preset입니다. follower나 stamp 수가 많으면 preview 비용이 늘 수 있습니다.";
        case RibbonCurveQuality::Custom:
            return "아래 세부 곡선 값을 직접 편집합니다.";
        default:
            return "";
        }
    }

    const char* Get_SourceHistorySpriteTrailStampSpawnModeLabel(SourceHistorySpriteTrailStampSpawnMode value)
    {
        switch (value)
        {
        case SourceHistorySpriteTrailStampSpawnMode::Distance:
            return "Distance";
        case SourceHistorySpriteTrailStampSpawnMode::Time:
            return "Time";
        default:
            return "Distance";
        }
    }

    bool Draw_SourceHistorySpriteTrailStampSpawnModeProperty(
        DetailPropertyContext& detailContext,
        SourceHistorySpriteTrailStampSpawnMode& value,
        SourceHistorySpriteTrailStampSpawnMode defaultValue)
    {
        bool changed = false;
        ImGui::PushID("SourceHistorySpriteTrailStampSpawnMode");
        detailContext.Draw_PropertyLabel("스탬프 생성 기준", "Distance는 source path 이동 거리, Time은 시간 간격으로 sprite card를 찍습니다.");
        if (ImGui::BeginCombo("##Value", Get_SourceHistorySpriteTrailStampSpawnModeLabel(value)))
        {
            constexpr SourceHistorySpriteTrailStampSpawnMode kValues[] = {
                SourceHistorySpriteTrailStampSpawnMode::Distance,
                SourceHistorySpriteTrailStampSpawnMode::Time,
            };
            for (const SourceHistorySpriteTrailStampSpawnMode candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_SourceHistorySpriteTrailStampSpawnModeLabel(candidate), selected))
                {
                    value = candidate;
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

    const char* Get_HistoryBudgetPresetLabel(HistoryBudgetPreset value)
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

    const char* Get_HistoryBudgetPriorityLabel(HistoryBudgetPriority value)
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

    const char* Get_HistoryBudgetDensityBiasLabel(HistoryBudgetDensityBias value)
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

    void Draw_HistoryBudgetLink(const Shared<Emitter_View>& emitterView, const AuthoringEmitter& emitter)
    {
        if (emitterView == nullptr)
            return;

        ImGui::TextDisabled(
            "History Budget: %s / %s / %s",
            Get_HistoryBudgetPresetLabel(emitterView->Get_HistoryBudget().preset),
            Get_HistoryBudgetPriorityLabel(emitter.historyBudget.priority),
            Get_HistoryBudgetDensityBiasLabel(emitter.historyBudget.densityBias)
        );
        ImGui::SameLine();
        if (ImGui::SmallButton("Open History Budget"))
        {
            const Shared<Editor_Window> historyBudgetWindow =
                EDITOR != nullptr ? EDITOR->Get_Window(L"History Budget") : nullptr;
            const Shared<HistoryBudget_View> historyBudgetView =
                dynamic_pointer_cast<HistoryBudget_View>(historyBudgetWindow);
            if (historyBudgetView != nullptr)
                historyBudgetView->Open_Target(emitter.id);
        }
    }

    const char* Get_SourceHistoryRibbonSourceModeLabel(SourceHistoryRibbonSourceMode value)
    {
        switch (value)
        {
        case SourceHistoryRibbonSourceMode::SelfRoot:
            return "Self Root";
        case SourceHistoryRibbonSourceMode::ParticleEmitter:
            return "Particle Emitter";
        case SourceHistoryRibbonSourceMode::SourceEmitter:
            return "Source Emitter (Legacy)";
        default:
            return "Self Root";
        }
    }

    const char* Get_BeamEndpointModeLabel(BeamEndpointMode value)
    {
        switch (value)
        {
        case BeamEndpointMode::StartEnd:
            return "시작 / 끝";
        case BeamEndpointMode::DirectionLength:
            return "방향 / 길이";
        default:
            return "시작 / 끝";
        }
    }

    const char* Get_BeamBranchPresetLabel(BeamBranchPreset value)
    {
        switch (value)
        {
        case BeamBranchPreset::EndGuided:
            return "끝점 유도";
        case BeamBranchPreset::DownStrike:
            return "내려꽂힘";
        case BeamBranchPreset::Entangle:
            return "얽힘";
        case BeamBranchPreset::ShortCrack:
            return "짧은 균열";
        default:
            return "끝점 유도";
        }
    }

    bool Draw_BeamBranchPresetProperty(
        DetailPropertyContext& detailContext,
        BeamBranchPreset& value,
        BeamBranchPreset defaultValue)
    {
        static constexpr BeamBranchPreset values[] = {
            BeamBranchPreset::EndGuided,
            BeamBranchPreset::DownStrike,
            BeamBranchPreset::Entangle,
            BeamBranchPreset::ShortCrack,
        };

        ImGui::PushID("BeamBranchPreset");
        detailContext.Draw_PropertyLabel("가지 형태", "branch가 parent 중간에서 시작해 어떤 목표점으로 수렴할지 선택합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_BeamBranchPresetLabel(value)))
        {
            for (const BeamBranchPreset candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_BeamBranchPresetLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        ImGui::PopID();
        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }
        return changed;
    }

    bool Draw_BeamEndpointModeProperty(
        DetailPropertyContext& detailContext,
        BeamEndpointMode& value,
        BeamEndpointMode defaultValue)
    {
        static constexpr BeamEndpointMode values[] = {
            BeamEndpointMode::StartEnd,
            BeamEndpointMode::DirectionLength,
        };

        ImGui::PushID("BeamEndpointMode");
        detailContext.Draw_PropertyLabel("끝점 방식", "Beam path를 로컬 시작/끝 또는 방향/길이 중 어떤 방식으로 만들지 선택합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_BeamEndpointModeLabel(value)))
        {
            for (const BeamEndpointMode candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_BeamEndpointModeLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        ImGui::PopID();
        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }
        return changed;
    }

    const char* Get_RenderLayerOverrideLabel(EmitterRenderLayerOverride value)
    {
        switch (value)
        {
        case EmitterRenderLayerOverride::UIEffect:
            return "UIEffect";
        case EmitterRenderLayerOverride::Auto:
        default:
            return "Auto";
        }
    }

    const char* Get_TrailPreviewGateModeLabel(TrailPreviewGateMode value)
    {
        switch (value)
        {
        case TrailPreviewGateMode::Always:
            return "항상";
        case TrailPreviewGateMode::Window:
            return "구간";
        default:
            return "항상";
        }
    }

    const char* Get_TrailPreviewCharacterSlotLabel(TrailPreviewCharacterSlot value)
    {
        switch (value)
        {
        case TrailPreviewCharacterSlot::Player:
            return "Player";
        case TrailPreviewCharacterSlot::Monster:
            return "Monster";
        default:
            return "Player";
        }
    }

    const char* Get_TrailPreviewMotionLabel(TrailPreviewMotion value)
    {
        switch (value)
        {
        case TrailPreviewMotion::Auto:
            return "Auto";
        case TrailPreviewMotion::Manual:
            return "Manual";
        default:
            return "Auto";
        }
    }

    void Get_TrailPreviewAnimationNames(vector<string>& outNames)
    {
        outNames.clear();

        const Shared<Level> currentLevel = GAME->Current_LevelType();
        const Shared<Level_EffectEditor> effectEditorLevel = dynamic_pointer_cast<Level_EffectEditor>(currentLevel);
        if (effectEditorLevel == nullptr)
            return;

        effectEditorLevel->Get_TrailPreviewAnimationNames(outNames);
    }

    bool Draw_TrailPreviewCharacterSlotProperty(
        DetailPropertyContext& detailContext,
        TrailPreviewCharacterSlot& value)
    {
        static constexpr TrailPreviewCharacterSlot values[] = {
            TrailPreviewCharacterSlot::Player,
            TrailPreviewCharacterSlot::Monster,
        };

        ImGui::PushID("TrailPreviewCharacterSlot");
        detailContext.Draw_PropertyLabel("프리뷰 대상", "Player는 기존 Gustave/Lanceram, Monster는 Simon preview fixture를 사용합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_TrailPreviewCharacterSlotLabel(value)))
        {
            for (const TrailPreviewCharacterSlot candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_TrailPreviewCharacterSlotLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != TrailPreviewCharacterSlot::Player);
        ImGui::PopID();

        if (resetClicked)
        {
            value = TrailPreviewCharacterSlot::Player;
            return true;
        }

        return changed;
    }

    bool Draw_TrailPreviewMotionProperty(
        DetailPropertyContext& detailContext,
        TrailPreviewMotion& value)
    {
        static constexpr TrailPreviewMotion values[] = {
            TrailPreviewMotion::Auto,
            TrailPreviewMotion::Manual,
        };

        ImGui::PushID("TrailPreviewMotion");
        detailContext.Draw_PropertyLabel("모션 모드", "Auto는 기존 Play/Edit preview 동작을 유지하고, Manual은 선택한 animation의 normalized time을 gate 기준으로 씁니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_TrailPreviewMotionLabel(value)))
        {
            for (const TrailPreviewMotion candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_TrailPreviewMotionLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != TrailPreviewMotion::Auto);
        ImGui::PopID();

        if (resetClicked)
        {
            value = TrailPreviewMotion::Auto;
            return true;
        }

        return changed;
    }

    bool Draw_TrailPreviewManualAnimationProperty(
        DetailPropertyContext& detailContext,
        string& value)
    {
        vector<string> animationNames;
        Get_TrailPreviewAnimationNames(animationNames);

        const string previewLabel = value.empty()
                                    ? "애니메이션 선택"
                                    : value;

        ImGui::PushID("TrailPreviewManualAnimation");
        detailContext.Draw_PropertyLabel("수동 애니메이션", "현재 preview 대상 body model에 로드된 animation입니다. 선택한 animation의 normalized time이 gate 기준입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", previewLabel.c_str()))
        {
            for (const string& animationName : animationNames)
            {
                const bool selected = value == animationName;
                if (ImGui::Selectable(animationName.c_str(), selected))
                {
                    value = animationName;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            if (animationNames.empty())
                ImGui::TextDisabled("트레일 프리뷰를 켜면 animation list를 읽을 수 있습니다.");

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(!value.empty());
        ImGui::PopID();

        if (resetClicked)
        {
            value.clear();
            return true;
        }

        return changed;
    }

    bool Draw_TrailPreviewGateModeProperty(
        DetailPropertyContext& detailContext,
        TrailPreviewGateMode& value,
        TrailPreviewGateMode defaultValue,
        const char* id)
    {
        static constexpr TrailPreviewGateMode values[] = {
            TrailPreviewGateMode::Always,
            TrailPreviewGateMode::Window,
        };

        ImGui::PushID(id);
        detailContext.Draw_PropertyLabel("활성 방식", "트레일 프리뷰 sample을 항상 받거나 Play 타임라인 비율 구간에서만 받습니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_TrailPreviewGateModeLabel(value)))
        {
            for (const TrailPreviewGateMode candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_TrailPreviewGateModeLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        return changed;
    }

    bool Draw_TrailPreviewGateRatioProperty(
        DetailPropertyContext& detailContext,
        const char* id,
        const char* label,
        float& value,
        float defaultValue,
        const char* tooltip)
    {
        ImGui::PushID(id);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = ImGui::DragFloat("##Value", &value, 0.001f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (detailContext.Draw_ResetButton(fabs(value - defaultValue) > 0.0001f))
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();

        if (changed)
            value = clamp(value, 0.f, 1.f);

        return changed;
    }

    bool Draw_RenderLayerOverrideProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EmitterRenderLayerOverride& value,
        EmitterRenderLayerOverride defaultValue)
    {
        static constexpr EmitterRenderLayerOverride values[] = {
            EmitterRenderLayerOverride::Auto,
            EmitterRenderLayerOverride::UIEffect,
        };

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_RenderLayerOverrideLabel(value)))
        {
            for (const EmitterRenderLayerOverride candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_RenderLayerOverrideLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        return changed;
    }

    bool Draw_SourceHistoryRibbonSourceModeProperty(
        DetailPropertyContext& detailContext,
        SourceHistoryRibbonSourceMode& value,
        SourceHistoryRibbonSourceMode defaultValue)
    {
        static constexpr SourceHistoryRibbonSourceMode values[] = {
            SourceHistoryRibbonSourceMode::SelfRoot,
            SourceHistoryRibbonSourceMode::ParticleEmitter,
        };

        ImGui::PushID("SourceHistoryRibbonSourceMode");
        detailContext.Draw_PropertyLabel("소스 모드", "Particle Emitter는 같은 effect 내부 Sprite/Mesh particle을 follower source로 사용합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_SourceHistoryRibbonSourceModeLabel(value)))
        {
            for (const SourceHistoryRibbonSourceMode candidate : values)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_SourceHistoryRibbonSourceModeLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        if (value == SourceHistoryRibbonSourceMode::SourceEmitter)
            ImGui::TextDisabled("Legacy Source Emitter root follower value will be treated as Particle Emitter when a source id is set.");

        return changed;
    }

    bool Can_UseAsSourceHistoryRibbonParticleSource(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::None ||
               emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    }

    const char* Get_SourceHistoryRibbonParticleSourceTypeLabel(const AuthoringEmitter& emitter)
    {
        if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
            return "Mesh";

        return "Sprite";
    }

    string Build_SourceHistoryRibbonSourceEmitterLabel(const AuthoringEmitter& emitter)
    {
        string label = emitter.name.empty() ? "Emitter" : emitter.name;
        label += " (id: ";
        label += to_string(emitter.id);
        label += ", ";
        label += Get_SourceHistoryRibbonParticleSourceTypeLabel(emitter);
        if (!emitter.enabled)
            label += ", disabled";
        label += ")";
        return label;
    }

    string Resolve_SourceHistoryRibbonSourceEmitterPreviewLabel(
        const vector<AuthoringEmitter>& emitters,
        uint32 currentEmitterId,
        uint32 sourceEmitterId)
    {
        if (sourceEmitterId == 0u)
            return "None";

        for (const AuthoringEmitter& emitter : emitters)
        {
            if (emitter.id == sourceEmitterId)
            {
                return emitter.id == currentEmitterId
                       ? "Self reference is invalid"
                       : Build_SourceHistoryRibbonSourceEmitterLabel(emitter);
            }
        }

        return "Missing emitter (id: " + to_string(sourceEmitterId) + ")";
    }

    bool Draw_SourceHistoryRibbonSourceEmitterProperty(
        DetailPropertyContext& detailContext,
        const vector<AuthoringEmitter>& emitters,
        const AuthoringEmitter& currentEmitter,
        uint32& sourceEmitterId,
        uint32 defaultValue)
    {
        bool changed = false;
        const string previewLabel =
            Resolve_SourceHistoryRibbonSourceEmitterPreviewLabel(emitters, currentEmitter.id, sourceEmitterId);

        ImGui::PushID("SourceHistoryRibbonSourceEmitter");
        detailContext.Draw_PropertyLabel("소스 Emitter", "같은 effect 내부 Sprite/Mesh particle emitter를 선택합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::BeginCombo("##Value", previewLabel.c_str()))
        {
            const bool noneSelected = sourceEmitterId == 0u;
            if (ImGui::Selectable("None", noneSelected))
            {
                sourceEmitterId = 0u;
                changed = true;
            }
            if (noneSelected)
                ImGui::SetItemDefaultFocus();

            for (const AuthoringEmitter& candidate : emitters)
            {
                if (candidate.id == currentEmitter.id ||
                    !Can_UseAsSourceHistoryRibbonParticleSource(candidate))
                    continue;

                const string label = Build_SourceHistoryRibbonSourceEmitterLabel(candidate);
                const bool selected = sourceEmitterId == candidate.id;
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    sourceEmitterId = candidate.id;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(sourceEmitterId != defaultValue);
        ImGui::PopID();

        if (resetClicked)
        {
            sourceEmitterId = defaultValue;
            return true;
        }

        return changed;
    }

    TrailTypeData Build_TrailCurvePreset(TrailCurveQuality quality)
    {
        TrailTypeData preset{};

        switch (quality)
        {
        case TrailCurveQuality::Smooth:
            preset.sampleSpacing = 0.04f;
            preset.curveSubdivision = 8u;
            preset.smoothTangent = true;
            preset.sideFade = 0.12f;
            break;
        case TrailCurveQuality::HighQuality:
            preset.sampleSpacing = 0.02f;
            preset.curveSubdivision = 12u;
            preset.smoothTangent = true;
            preset.sideFade = 0.16f;
            break;
        case TrailCurveQuality::Basic:
        case TrailCurveQuality::Custom:
        default:
            preset.sampleSpacing = 0.08f;
            preset.curveSubdivision = 4u;
            preset.smoothTangent = true;
            preset.sideFade = 0.08f;
            break;
        }

        return preset;
    }

    RibbonTypeData Build_RibbonCurvePreset(RibbonCurveQuality quality)
    {
        RibbonTypeData preset{};

        switch (quality)
        {
        case RibbonCurveQuality::Smooth:
            preset.sampleSpacing = 0.04f;
            preset.curveSubdivision = 8u;
            preset.smoothTangent = false;
            preset.sampleInterval = 0.f;
            break;
        case RibbonCurveQuality::HighQuality:
            preset.sampleSpacing = 0.02f;
            preset.curveSubdivision = 12u;
            preset.smoothTangent = false;
            preset.sampleInterval = 0.f;
            break;
        case RibbonCurveQuality::Basic:
        case RibbonCurveQuality::Custom:
        default:
            preset.sampleSpacing = 0.08f;
            preset.curveSubdivision = 4u;
            preset.smoothTangent = false;
            preset.sampleInterval = 0.f;
            break;
        }

        return preset;
    }

    bool Apply_TrailCurvePreset(TrailTypeData& data)
    {
        if (data.curveQuality == TrailCurveQuality::Custom)
            return false;

        const TrailTypeData preset = Build_TrailCurvePreset(data.curveQuality);
        bool changed = false;

        if (data.sampleSpacing != preset.sampleSpacing)
        {
            data.sampleSpacing = preset.sampleSpacing;
            changed = true;
        }
        if (data.curveSubdivision != preset.curveSubdivision)
        {
            data.curveSubdivision = preset.curveSubdivision;
            changed = true;
        }
        if (data.smoothTangent != preset.smoothTangent)
        {
            data.smoothTangent = preset.smoothTangent;
            changed = true;
        }
        if (data.sideFade != preset.sideFade)
        {
            data.sideFade = preset.sideFade;
            changed = true;
        }

        return changed;
    }

    bool Apply_RibbonCurvePreset(RibbonTypeData& data)
    {
        if (data.curveQuality == RibbonCurveQuality::Custom)
            return false;

        const RibbonTypeData preset = Build_RibbonCurvePreset(data.curveQuality);
        bool changed = false;

        if (data.sampleSpacing != preset.sampleSpacing)
        {
            data.sampleSpacing = preset.sampleSpacing;
            changed = true;
        }
        if (data.curveSubdivision != preset.curveSubdivision)
        {
            data.curveSubdivision = preset.curveSubdivision;
            changed = true;
        }
        if (data.smoothTangent != preset.smoothTangent)
        {
            data.smoothTangent = preset.smoothTangent;
            changed = true;
        }
        if (data.sampleInterval != preset.sampleInterval)
        {
            data.sampleInterval = preset.sampleInterval;
            changed = true;
        }

        return changed;
    }

    SourceHistorySpriteTrailTypeData Build_SourceHistorySpriteTrailCurvePreset(RibbonCurveQuality quality)
    {
        SourceHistorySpriteTrailTypeData preset{};

        switch (quality)
        {
        case RibbonCurveQuality::Smooth:
            preset.sampleSpacing = 0.04f;
            preset.curveSubdivision = 8u;
            preset.smoothTangent = true;
            break;
        case RibbonCurveQuality::HighQuality:
            preset.sampleSpacing = 0.02f;
            preset.curveSubdivision = 12u;
            preset.smoothTangent = true;
            break;
        case RibbonCurveQuality::Basic:
        case RibbonCurveQuality::Custom:
        default:
            preset.sampleSpacing = 0.08f;
            preset.curveSubdivision = 4u;
            preset.smoothTangent = true;
            break;
        }

        return preset;
    }

    bool Apply_SourceHistorySpriteTrailCurvePreset(SourceHistorySpriteTrailTypeData& data)
    {
        if (data.curveQuality == RibbonCurveQuality::Custom)
            return false;

        const SourceHistorySpriteTrailTypeData preset = Build_SourceHistorySpriteTrailCurvePreset(data.curveQuality);
        bool changed = false;

        if (data.sampleSpacing != preset.sampleSpacing)
        {
            data.sampleSpacing = preset.sampleSpacing;
            changed = true;
        }
        if (data.curveSubdivision != preset.curveSubdivision)
        {
            data.curveSubdivision = preset.curveSubdivision;
            changed = true;
        }
        if (data.smoothTangent != preset.smoothTangent)
        {
            data.smoothTangent = preset.smoothTangent;
            changed = true;
        }

        return changed;
    }

    fs::path Resolve_AuthoredAssetPath(const string& guid, const string& path)
    {
        if (GAME == nullptr)
            return fs::path(String::ToWString(path));

        if (!guid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
            if (!resolvedPath.empty())
                return fs::path(resolvedPath).lexically_normal();
        }

        fs::path resolvedPath = fs::path(String::ToWString(path));
        if (resolvedPath.is_relative())
            resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

        return resolvedPath.lexically_normal();
    }

    string Build_MeshModelValidationMessage(const MeshTypeData& data, ImVec4& outColor)
    {
        outColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        if (data.modelGuid.empty() && data.modelPath.empty())
            return "Model asset is not assigned.";

        const fs::path resolvedPath = Resolve_AuthoredAssetPath(data.modelGuid, data.modelPath);
        if (resolvedPath.empty() || !fs::exists(resolvedPath) || !fs::is_regular_file(resolvedPath))
        {
            outColor = ImVec4{ 1.f, 0.55f, 0.25f, 1.f };
            return "Missing model asset: " + String::ToString(resolvedPath.wstring());
        }

        if (String::ToLowerCopy(resolvedPath.extension().string()) != ".model")
        {
            outColor = ImVec4{ 1.f, 0.35f, 0.35f, 1.f };
            return "Unsupported model file extension.";
        }

        if (!data.modelGuid.empty() && GAME != nullptr)
        {
            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(data.modelGuid);
            if (assetMeta == nullptr)
            {
                outColor = ImVec4{ 1.f, 0.55f, 0.25f, 1.f };
                return "Model guid is not registered in the asset registry.";
            }

            if (assetMeta->type != "Model")
            {
                outColor = ImVec4{ 1.f, 0.35f, 0.35f, 1.f };
                return "Assigned guid is not a Model asset.";
            }

            const string modelType = String::ToLowerCopy(assetMeta->modelType);
            if (!modelType.empty() && modelType != "nonanim" && modelType != "staticmesh")
            {
                outColor = ImVec4{ 1.f, 0.55f, 0.25f, 1.f };
                return "Unsupported model type for mesh emitter v1: " + assetMeta->modelType;
            }
        }

        outColor = ImVec4{ 0.45f, 0.9f, 0.55f, 1.f };
        return "Model asset is valid for authoring.";
    }

    bool Draw_MeshDataEntryButton(const MeshTypeData& data)
    {
        constexpr float kEntryHeight{ 52.f };
        const float width = max(180.f, ImGui::GetContentRegionAvail().x);
        const bool hasModel = !data.modelPath.empty();
        const bool hasMaterialSource = !data.assignedEffectMaterialPath.empty();

        ImGui::InvisibleButton("MeshDataEditEntry", ImVec2{ width, kEntryHeight });

        const ImVec2 minPos = ImGui::GetItemRectMin();
        const ImVec2 maxPos = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        const bool clicked = ImGui::IsItemClicked();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 fillColor = active
                                ? IM_COL32(38, 48, 64, 255)
                                : hovered ? IM_COL32(34, 42, 58, 255) : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = hovered ? IM_COL32(116, 156, 235, 255) : IM_COL32(70, 78, 92, 255);
        drawList->AddRectFilled(minPos, maxPos, fillColor, 6.f);
        drawList->AddRect(minPos, maxPos, borderColor, 6.f, 0, hovered ? 2.f : 1.f);

        const ImVec2 textMin{ minPos.x + 12.f, minPos.y + 8.f };
        drawList->AddText(textMin, IM_COL32(238, 241, 248, 255), ICON_FA_CUBES " MeshData 편집");
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 0.9f,
            ImVec2{ textMin.x, minPos.y + 30.f },
            IM_COL32(150, 160, 176, 255),
            "모델 / 머티리얼 설정"
        );

        const bool setupReady = hasModel && hasMaterialSource && data.hasAssignedMaterialInstance;
        const string status = width >= 420.f
                              ? string{ hasModel ? "Model OK" : "Model missing" } +
                                " | " +
                                (hasMaterialSource ? "Material OK" : "Material missing") +
                                " | " +
                                (data.hasAssignedMaterialInstance ? "Copy OK" : "Copy missing")
                              : setupReady ? string{ "Ready" } : string{ "Setup needed" };
        const ImVec2 statusSize = ImGui::CalcTextSize(status.c_str());
        const ImVec2 statusPos{
            max(textMin.x, maxPos.x - statusSize.x - 12.f),
            minPos.y + 18.f
        };
        const ImU32 statusColor =
            setupReady
            ? IM_COL32(105, 220, 135, 255)
            : IM_COL32(245, 165, 92, 255);
        drawList->AddText(statusPos, statusColor, status.c_str());

        if (hovered)
            ImGui::SetTooltip("MeshData Preview 창에서 model/material 조합과 내부 material copy를 편집합니다.");

        return clicked;
    }

    bool Has_PostProcessFeature(PostProcessFeature features, PostProcessFeature feature)
    {
        return Has_Flag(features, feature);
    }

    const char* Get_PostProcessPresetLabel(EffectEditorPostProcessPreset preset)
    {
        switch (preset)
        {
        case EffectEditorPostProcessPreset::PreviewFriendly:
            return "Preview Friendly";
        case EffectEditorPostProcessPreset::ClientCurrent:
            return "Client Current";
        case EffectEditorPostProcessPreset::Custom:
        default:
            return "Custom";
        }
    }

    const char* Get_ToneMapModeLabel(uint32 toneMapMode)
    {
        switch (toneMapMode)
        {
        case 0:
            return "Reinhard";
        case 1:
            return "ACES";
        case 2:
        default:
            return "Uncharted2";
        }
    }

    void Draw_PostProcessPresetProperty(DetailPropertyContext& detailContext)
    {
        if (EDITOR == nullptr)
            return;

        const EffectEditorPostProcessPreset currentPreset = EDITOR->Get_PostProcessPreset();

        ImGui::PushID("PostProcessPreset");
        detailContext.Draw_PropertyLabel("Preset", "프리뷰 후처리 설정 묶음입니다. editor preview 전용이며 effect asset에는 저장되지 않습니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PostProcessPresetLabel(currentPreset)))
        {
            const bool previewSelected = currentPreset == EffectEditorPostProcessPreset::PreviewFriendly;
            if (ImGui::Selectable("Preview Friendly", previewSelected))
                EDITOR->Apply_PostProcessOptions(EDITOR->Get_DefaultPostProcessOptions());

            if (previewSelected)
                ImGui::SetItemDefaultFocus();

            if (EDITOR->Has_ClientPostProcessSnapshot())
            {
                const bool clientSelected = currentPreset == EffectEditorPostProcessPreset::ClientCurrent;
                if (ImGui::Selectable("Client Current", clientSelected))
                    EDITOR->Apply_PostProcessOptions(EDITOR->Get_ClientPostProcessSnapshot());

                if (clientSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
    }

    bool Draw_ToneMapModeProperty(DetailPropertyContext& detailContext, uint32& toneMapMode, uint32 defaultToneMapMode)
    {
        static constexpr uint32 toneMapModes[] = { 0u, 1u, 2u };

        ImGui::PushID("ToneMapMode");
        detailContext.Draw_PropertyLabel("ToneMap Mode", "HDR 색을 LDR 화면 범위로 눌러 담는 tone mapping 곡선입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_ToneMapModeLabel(toneMapMode)))
        {
            for (const uint32 candidate : toneMapModes)
            {
                const bool selected = toneMapMode == candidate;
                if (ImGui::Selectable(Get_ToneMapModeLabel(candidate), selected))
                {
                    toneMapMode = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(toneMapMode != defaultToneMapMode);
        ImGui::PopID();

        if (resetClicked)
        {
            toneMapMode = defaultToneMapMode;
            return true;
        }

        return changed;
    }

    bool Draw_TrailCurveQualityProperty(DetailPropertyContext& detailContext, TrailTypeData& data, const TrailTypeData& defaultData)
    {
        bool changed = false;

        ImGui::PushID("TrailCurveQuality");
        detailContext.Draw_PropertyLabel("곡선 품질", "트레일 곡선 샘플 밀도와 가장자리 페이드 preset입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_TrailCurveQualityLabel(data.curveQuality)))
        {
            for (const TrailCurveQuality value : kTrailCurveQualityValues)
            {
                const bool selected = data.curveQuality == value;
                if (ImGui::Selectable(Get_TrailCurveQualityLabel(value), selected))
                {
                    data.curveQuality = value;
                    changed = true;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", Get_TrailCurveQualityTooltip(value));

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(data.curveQuality != defaultData.curveQuality))
        {
            data.curveQuality = defaultData.curveQuality;
            changed = true;
        }
        ImGui::PopID();

        if (changed)
            changed |= Apply_TrailCurvePreset(data);

        return changed;
    }

    bool Draw_RibbonCurveQualityProperty(DetailPropertyContext& detailContext, RibbonTypeData& data, const RibbonTypeData& defaultData)
    {
        bool changed = false;

        ImGui::PushID("RibbonCurveQuality");
        detailContext.Draw_PropertyLabel("품질", "리본 sample 밀도 preset입니다. 사용자 지정에서만 세부 sample 값을 직접 편집합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_RibbonCurveQualityLabel(data.curveQuality)))
        {
            for (const RibbonCurveQuality value : kRibbonCurveQualityValues)
            {
                const bool selected = data.curveQuality == value;
                if (ImGui::Selectable(Get_RibbonCurveQualityLabel(value), selected))
                {
                    data.curveQuality = value;
                    changed = true;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", Get_RibbonCurveQualityTooltip(value));

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(data.curveQuality != defaultData.curveQuality))
        {
            data.curveQuality = defaultData.curveQuality;
            changed = true;
        }
        ImGui::PopID();

        if (changed)
            changed |= Apply_RibbonCurvePreset(data);

        return changed;
    }

    bool Draw_SourceHistorySpriteTrailCurveQualityProperty(
        DetailPropertyContext& detailContext,
        SourceHistorySpriteTrailTypeData& data,
        const SourceHistorySpriteTrailTypeData& defaultData)
    {
        bool changed = false;

        ImGui::PushID("SourceHistorySpriteTrailCurveQuality");
        detailContext.Draw_PropertyLabel("곡선 품질", "source history path의 곡선 보간 품질 preset입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_RibbonCurveQualityLabel(data.curveQuality)))
        {
            for (const RibbonCurveQuality value : kRibbonCurveQualityValues)
            {
                const bool selected = data.curveQuality == value;
                if (ImGui::Selectable(Get_RibbonCurveQualityLabel(value), selected))
                {
                    data.curveQuality = value;
                    changed = true;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", Get_RibbonCurveQualityTooltip(value));
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(data.curveQuality != defaultData.curveQuality))
        {
            data.curveQuality = defaultData.curveQuality;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }
}

Detail_View::Detail_View()
    : Editor_Window{ L"Detail", ICON_FA_PEN_TO_SQUARE }
{
}

Detail_View::~Detail_View()
{
    Free();
}

void Detail_View::Update(float timeDelta)
{
    __super::Update(timeDelta);
    _detailTimeDelta = max(0.f, timeDelta);
}

void Detail_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Render_EffectAuthoringDetail();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Detail_View::Render_EffectAuthoringDetail()
{
    if (EDITOR == nullptr || EDITOR->Get_EditorContext() == nullptr)
    {
        ImGui::TextDisabled("Editor context is not available.");
        return;
    }

    const EffectAuthoringSelection& selection = EDITOR->Get_EditorContext()->Get_EffectSelection();
    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (emitterView == nullptr)
    {
        ImGui::TextDisabled("Emitter view is not available.");
        return;
    }

    Commit_PendingAuthoringEditOnTargetChange(emitterView, selection);

    if (selection.kind == EffectAuthoringSelectionKind::None)
    {
        Render_ParticleSystemDetail(emitterView, selection, emitterView->Get_ParticleSystemData());
        return;
    }

    AuthoringEmitter* emitter = emitterView->Find_Emitter(selection.emitterId);
    if (nullptr == emitter)
    {
        Commit_PendingAuthoringEdit(emitterView);
        ImGui::TextDisabled("Selected emitter is not available.");
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::Emitter)
    {
        Render_EmitterDetail(emitterView, selection, *emitter);
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::TypeData)
    {
        Render_TypeDataDetail(emitterView, selection, *emitter);
        return;
    }

    AuthoringModule* module = emitterView->Find_Module(selection.emitterId, selection.moduleId);
    if (nullptr == module)
    {
        Commit_PendingAuthoringEdit(emitterView);
        ImGui::TextDisabled("Selected module is not available.");
        return;
    }

    Render_ModuleDetail(emitterView, selection, *emitter, *module);
}

void Detail_View::Render_ParticleSystemDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection&,
    ParticleSystemAuthoringData&)
{
    ImGui::TextUnformatted("파티클 시스템");
    ImGui::Separator();

    if (_detailPropertyContext.Begin_PropertyTable("ParticleSystem"))
    {
        if (EDITOR != nullptr)
        {
            bool autoReplayPreviewEnabled = EDITOR->Is_AutoReplayPreviewEnabled();
            if (_detailPropertyContext.Draw_BoolProperty(
                "자동 재생 (프리뷰)",
                autoReplayPreviewEnabled,
                false,
                "에디터 프리뷰 전용 상태입니다. 켜면 authoring 값을 바꿀 때 preview 재생을 자동으로 다시 시작합니다."
            ))
                EDITOR->Set_AutoReplayPreviewEnabled(autoReplayPreviewEnabled);
        }
        if (EDITOR != nullptr)
        {
            bool defaultSkyboxEnabled = EDITOR->Is_DefaultSkyboxEnabled();
            if (_detailPropertyContext.Draw_BoolProperty(
                "Skybox",
                defaultSkyboxEnabled,
                true,
                "에디터 프리뷰 전용 배경 모드입니다. effect asset에는 저장되지 않습니다."
            ))
                EDITOR->Set_DefaultSkyboxEnabled(defaultSkyboxEnabled);
        }
        if (EDITOR != nullptr)
        {
            bool previewPlaneVisible = EDITOR->Is_PreviewPlaneVisible();
            if (_detailPropertyContext.Draw_BoolProperty(
                "Preview Plane",
                previewPlaneVisible,
                false,
                "에디터 프리뷰 전용 바닥/기준면 표시입니다. effect asset에는 저장되지 않습니다."
            ))
                EDITOR->Set_PreviewPlaneVisible(previewPlaneVisible);
        }
        _detailPropertyContext.End_PropertyTable();
    }

    Render_TrailPreviewDetail();
    Render_PostProcessShaderDetail();
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TrailPreviewDetail()
{
    if (EDITOR == nullptr)
        return;

    if (!ImGui::CollapsingHeader("트레일 프리뷰", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    Vec3 baseLocalOffset = EDITOR->Get_TrailPreviewBaseLocalOffset();
    Vec3 tipLocalOffset = EDITOR->Get_TrailPreviewTipLocalOffset();
    Vec3 sourceLocalOffset = EDITOR->Get_SourceHistorySpriteTrailPreviewSourceLocalOffset();
    const Vec3 defaultBaseLocalOffset = EDITOR->Get_DefaultTrailPreviewBaseLocalOffset();
    const Vec3 defaultTipLocalOffset = EDITOR->Get_DefaultTrailPreviewTipLocalOffset();
    const Vec3 defaultSourceLocalOffset = EDITOR->Get_DefaultSourceHistorySpriteTrailPreviewSourceLocalOffset();
    bool visible = EDITOR->Is_TrailPreviewVisible();
    bool debugRenderEnabled = EDITOR->Is_TrailPreviewDebugRenderEnabled();
    TrailPreviewCharacterSlot characterSlot = EDITOR->Get_TrailPreviewCharacterSlot();
    TrailPreviewMotion motion = EDITOR->Get_TrailPreviewMotion();
    string manualAnimationName = EDITOR->Get_TrailPreviewManualAnimationName();
    float animationSpeed = EDITOR->Get_TrailPreviewAnimationSpeed();
    TrailPreviewGateMode gateMode = EDITOR->Get_TrailPreviewGateMode();
    float gateStartRatio = EDITOR->Get_TrailPreviewGateStartRatio();
    float gateEndRatio = EDITOR->Get_TrailPreviewGateEndRatio();
    TrailPreviewGateMode spriteTrailGateMode = EDITOR->Get_SourceHistorySpriteTrailPreviewGateMode();
    float spriteTrailGateStartRatio = EDITOR->Get_SourceHistorySpriteTrailPreviewGateStartRatio();
    float spriteTrailGateEndRatio = EDITOR->Get_SourceHistorySpriteTrailPreviewGateEndRatio();

    if (_detailPropertyContext.Begin_PropertyTable("TrailPreviewCommon"))
    {
        if (_detailPropertyContext.Draw_BoolProperty("트레일 프리뷰", visible, false, "에디터 프리뷰 helper를 표시합니다. effect asset의 저장 데이터가 아닙니다."))
            EDITOR->Set_TrailPreviewVisible(visible);
        if (_detailPropertyContext.Draw_BoolProperty("디버그 렌더", debugRenderEnabled, true, "프리뷰 helper의 기준점과 샘플 흐름을 확인하기 위한 editor-only 표시입니다."))
            EDITOR->Set_TrailPreviewDebugRenderEnabled(debugRenderEnabled);
        if (Draw_TrailPreviewCharacterSlotProperty(_detailPropertyContext, characterSlot))
            EDITOR->Set_TrailPreviewCharacterSlot(characterSlot);
        if (Draw_TrailPreviewMotionProperty(_detailPropertyContext, motion))
            EDITOR->Set_TrailPreviewMotion(motion);
        if (motion == TrailPreviewMotion::Manual &&
            Draw_TrailPreviewManualAnimationProperty(_detailPropertyContext, manualAnimationName))
            EDITOR->Set_TrailPreviewManualAnimationName(manualAnimationName);
        static constexpr AuthoringValueRange kAnimationSpeedRange{ true, true, 0.f, 10.f };
        if (_detailPropertyContext.Draw_FloatProperty(
            "애니메이션 배속",
            animationSpeed,
            EDITOR->Get_DefaultTrailPreviewAnimationSpeed(),
            0.01f,
            "Trail Preview 플레이어 애니메이션 재생 배속입니다. 0이면 현재 프레임에서 멈춥니다.",
            &kAnimationSpeedRange
        ))
            EDITOR->Set_TrailPreviewAnimationSpeed(animationSpeed);
        _detailPropertyContext.End_PropertyTable();
    }

    if (ImGui::TreeNodeEx("트레일", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        bool trailLocalChanged = false;
        if (_detailPropertyContext.Begin_PropertyTable("TrailPreviewTrail"))
        {
            trailLocalChanged |= _detailPropertyContext.Draw_Vec3Property(
                "Base 로컬",
                baseLocalOffset,
                defaultBaseLocalOffset,
                0.005f,
                nullptr,
                "Trail preview helper의 base 기준점입니다. editor preview 전용 값입니다."
            );
            trailLocalChanged |= _detailPropertyContext.Draw_Vec3Property(
                "Tip 로컬",
                tipLocalOffset,
                defaultTipLocalOffset,
                0.005f,
                nullptr,
                "Trail preview helper의 tip 기준점입니다. editor preview 전용 값입니다."
            );
            if (Draw_TrailPreviewGateModeProperty(_detailPropertyContext, gateMode, TrailPreviewGateMode::Window, "TrailPreviewTrailGateMode"))
                EDITOR->Set_TrailPreviewGateMode(gateMode);
            if (gateMode == TrailPreviewGateMode::Window)
            {
                bool gateWindowChanged = false;
                gateWindowChanged |= Draw_TrailPreviewGateRatioProperty(
                    _detailPropertyContext,
                    "TrailPreviewTrailGateStartRatio",
                    "시작 비율",
                    gateStartRatio,
                    EDITOR->Get_DefaultTrailPreviewGateStartRatio(),
                    "현재 선택된 애니메이션의 normalized time 기준입니다. 타이밍이 맞지 않으면 항상으로 바꾸거나 시작/종료 비율을 조정하세요."
                );
                gateWindowChanged |= Draw_TrailPreviewGateRatioProperty(
                    _detailPropertyContext,
                    "TrailPreviewTrailGateEndRatio",
                    "종료 비율",
                    gateEndRatio,
                    EDITOR->Get_DefaultTrailPreviewGateEndRatio(),
                    "현재 선택된 애니메이션의 normalized time 기준입니다. 시작 비율보다 작으면 loop 초반에서 닫힙니다."
                );
                if (gateWindowChanged)
                    EDITOR->Set_TrailPreviewGateWindow(gateStartRatio, gateEndRatio);
            }
            _detailPropertyContext.End_PropertyTable();
        }

        if (trailLocalChanged)
            EDITOR->Set_TrailPreviewLocalOffsets(baseLocalOffset, tipLocalOffset);

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("리본/스프라이트 트레일", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        bool sourceLocalChanged = false;
        if (_detailPropertyContext.Begin_PropertyTable("TrailPreviewSourceHistorySpriteTrail"))
        {
            sourceLocalChanged |= _detailPropertyContext.Draw_Vec3Property(
                "Source 로컬",
                sourceLocalOffset,
                defaultSourceLocalOffset,
                0.005f,
                nullptr,
                "Ribbon/SourceHistorySpriteTrail preview source 위치입니다. editor preview 전용 값입니다."
            );
            if (Draw_TrailPreviewGateModeProperty(
                _detailPropertyContext,
                spriteTrailGateMode,
                TrailPreviewGateMode::Window,
                "TrailPreviewSourceHistorySpriteTrailGateMode"
            ))
                EDITOR->Set_SourceHistorySpriteTrailPreviewGateMode(spriteTrailGateMode);

            if (spriteTrailGateMode == TrailPreviewGateMode::Window)
            {
                bool gateWindowChanged = false;
                gateWindowChanged |= Draw_TrailPreviewGateRatioProperty(
                    _detailPropertyContext,
                    "TrailPreviewSourceHistorySpriteTrailGateStartRatio",
                    "시작 비율",
                    spriteTrailGateStartRatio,
                    EDITOR->Get_DefaultSourceHistorySpriteTrailPreviewGateStartRatio(),
                    "현재 선택된 애니메이션의 normalized time 기준입니다. 타이밍이 맞지 않으면 항상으로 바꾸거나 시작/종료 비율을 조정하세요."
                );
                gateWindowChanged |= Draw_TrailPreviewGateRatioProperty(
                    _detailPropertyContext,
                    "TrailPreviewSourceHistorySpriteTrailGateEndRatio",
                    "종료 비율",
                    spriteTrailGateEndRatio,
                    EDITOR->Get_DefaultSourceHistorySpriteTrailPreviewGateEndRatio(),
                    "현재 선택된 애니메이션의 normalized time 기준입니다. 시작 비율보다 작으면 loop 초반에서 닫힙니다."
                );
                if (gateWindowChanged)
                    EDITOR->Set_SourceHistorySpriteTrailPreviewGateWindow(spriteTrailGateStartRatio, spriteTrailGateEndRatio);
            }

            _detailPropertyContext.End_PropertyTable();
        }

        if (sourceLocalChanged)
            EDITOR->Set_SourceHistorySpriteTrailPreviewSourceLocalOffset(sourceLocalOffset);

        ImGui::TreePop();
    }
}

void Detail_View::Render_PostProcessShaderDetail()
{
    if (EDITOR == nullptr)
        return;

    if (!ImGui::CollapsingHeader("후처리 셰이더", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const EffectEditorPostProcessOptions options = EDITOR->Get_PostProcessOptions();
    const EffectEditorPostProcessOptions defaultOptions = EDITOR->Get_DefaultPostProcessOptions();

    bool hdr = Has_PostProcessFeature(options.features, PostProcessFeature::HDR);
    bool exposure = Has_PostProcessFeature(options.features, PostProcessFeature::Exposure);
    bool toneMapping = Has_PostProcessFeature(options.features, PostProcessFeature::ToneMapping);
    bool bloom = Has_PostProcessFeature(options.features, PostProcessFeature::Bloom);
    bool gamma = Has_PostProcessFeature(options.features, PostProcessFeature::Gamma);

    const bool defaultHdr = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::HDR);
    const bool defaultExposure = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Exposure);
    const bool defaultToneMapping = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::ToneMapping);
    const bool defaultBloom = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Bloom);
    const bool defaultGamma = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Gamma);

    if (_detailPropertyContext.Begin_PropertyTable("PostProcessPreset"))
    {
        Draw_PostProcessPresetProperty(_detailPropertyContext);
        _detailPropertyContext.End_PropertyTable();
    }

    if (_detailPropertyContext.Begin_PropertyTable("PostProcessFeatures"))
    {
        if (_detailPropertyContext.Draw_BoolProperty("HDR", hdr, defaultHdr, "에디터 프리뷰 후처리 전용 토글입니다. HDR render target 경로를 사용합니다."))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::HDR, hdr);
        if (_detailPropertyContext.Draw_BoolProperty("Exposure", exposure, defaultExposure, "프리뷰 후처리에서 exposure 보정을 적용할지 정합니다."))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::Exposure, exposure);
        if (_detailPropertyContext.Draw_BoolProperty("ToneMapping", toneMapping, defaultToneMapping, "프리뷰 후처리에서 HDR 색을 화면 출력 범위로 매핑합니다."))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::ToneMapping, toneMapping);
        if (_detailPropertyContext.Draw_BoolProperty("Bloom", bloom, defaultBloom, "프리뷰 후처리에서 밝은 영역 번짐을 적용합니다."))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::Bloom, bloom);
        if (_detailPropertyContext.Draw_BoolProperty("Gamma", gamma, defaultGamma, "프리뷰 후처리에서 gamma 보정을 적용합니다."))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::Gamma, gamma);
        _detailPropertyContext.End_PropertyTable();
    }

    PostProcessCB params = EDITOR->Get_PostProcessOptions().params;
    bool paramsChanged = false;
    if (_detailPropertyContext.Begin_PropertyTable("PostProcessParams"))
    {
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Exposure",
            params.exposure,
            defaultOptions.params.exposure,
            0.01f,
            "프리뷰 화면 밝기를 조절하는 exposure 값입니다."
        );
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Inv White Point",
            params.invWhitePoint,
            defaultOptions.params.invWhitePoint,
            0.01f,
            "tone mapping에서 흰색 기준점을 보정하는 역 white point 값입니다."
        );
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Gamma", params.gamma, defaultOptions.params.gamma, 0.01f, "프리뷰 출력 gamma 보정 값입니다.");
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Bloom Radius",
            params.bloomRadius,
            defaultOptions.params.bloomRadius,
            0.01f,
            "Bloom blur가 퍼지는 반경입니다."
        );
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Bloom Threshold",
            params.bloomThreshold,
            defaultOptions.params.bloomThreshold,
            0.01f,
            "Bloom 후보가 되는 밝기 임계값입니다."
        );
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Bloom Intensity",
            params.bloomIntensity,
            defaultOptions.params.bloomIntensity,
            0.01f,
            "Bloom 결과를 최종 화면에 더하는 강도입니다."
        );
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Bloom Soft Knee",
            params.bloomSoftKnee,
            defaultOptions.params.bloomSoftKnee,
            0.01f,
            "Threshold 근처 Bloom 전환을 부드럽게 만드는 값입니다."
        );
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty(
            "Bloom Clamp",
            params.bloomClamp,
            defaultOptions.params.bloomClamp,
            0.05f,
            "Bloom 입력 밝기의 상한을 제한합니다."
        );
        paramsChanged |= Draw_ToneMapModeProperty(_detailPropertyContext, params.toneMapMode, defaultOptions.params.toneMapMode);
        _detailPropertyContext.End_PropertyTable();
    }

    if (paramsChanged)
        EDITOR->Set_PostProcessParams(params);
}

void Detail_View::Render_EmitterDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("파티클");
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    const AuthoringEmitter defaultData{};
    bool changed = false;

    if (_detailPropertyContext.Begin_PropertyTable("Emitter"))
    {
        changed |= _detailPropertyContext.Draw_StringProperty("이미터 이름", emitter.name, defaultData.name);

        ImGui::PushID("RendererType");
        _detailPropertyContext.Draw_PropertyLabel("렌더러 타입");
        ImGui::TextDisabled("%s", emitter.rendererType.c_str());
        _detailPropertyContext.Draw_ResetButton(false);
        ImGui::PopID();

        changed |= Draw_RenderLayerOverrideProperty(
            _detailPropertyContext,
            "렌더 레이어",
            emitter.renderLayerOverride,
            defaultData.renderLayerOverride
        );

        _detailPropertyContext.End_PropertyTable();
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Emitter Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TypeDataDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("타입 데이터");
    ImGui::Separator();

    bool changed = false;
    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    switch (emitter.typeData.kind)
    {
    case AuthoringTypeDataKind::Trail:
    {
        TrailTypeData* data = get_if<TrailTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("Trail TypeData payload is invalid.");
            break;
        }

        const TrailTypeData defaultData{};
        if (ImGui::CollapsingHeader("기본", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Draw_HistoryBudgetLink(emitterView, emitter);
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataBase"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "폭 비율",
                    data->width,
                    defaultData.width,
                    0.01f,
                    "Base/Tip 사이 길이에 곱할 트레일 strip 폭 비율입니다. 1이면 원본 폭을 유지합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "세그먼트 수명",
                    data->segmentLifetime,
                    defaultData.segmentLifetime,
                    0.01f,
                    "trail sample이 유지되는 시간입니다. 길수록 잔상이 오래 남습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "UV 타일링",
                    data->uvTiling,
                    defaultData.uvTiling,
                    0.01f,
                    "trail noise/breakup texture의 길이 방향 반복 배율입니다. Main/Mask body는 표시 길이에 맞춰 stretch됩니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "최대 표시 길이",
                    data->maxTrailLength,
                    defaultData.maxTrailLength,
                    0.01f,
                    "head 기준 누적 거리로 제한하는 최대 표시 길이입니다. 0 이하면 제한하지 않습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "꼬리 페이드 길이",
                    data->tailFadeLength,
                    defaultData.tailFadeLength,
                    0.01f,
                    "최대 표시 길이 끝으로 갈수록 alpha를 줄이는 거리입니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "자동 수명 페이드",
                    data->autoLifeFade,
                    defaultData.autoLifeFade,
                    "trail sample age 기준 alpha fade를 적용할지 결정합니다. 꺼도 세그먼트 수명이 지나면 sample은 제거됩니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "폭 가장자리 페이드",
                    data->sideFade,
                    defaultData.sideFade,
                    0.005f,
                    "trail 폭 방향 양끝 alpha를 줄이는 강도입니다. 높일수록 두꺼운 벽 느낌이 줄어듭니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
        }

        if (ImGui::CollapsingHeader("히스토리 진단"))
        {
            ImGui::TextDisabled("비용성 history 기준값은 History Budget View에서 source group 단위로 확정합니다.");
            ImGui::BeginDisabled();
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataCurveQuality"))
            {
                _detailPropertyContext.Draw_UintProperty(
                    "히스토리 수",
                    data->historyCount,
                    defaultData.historyCount,
                    "저장할 trail sample 개수입니다. 많을수록 긴 궤적을 유지할 수 있습니다."
                );
                Draw_TrailCurveQualityProperty(_detailPropertyContext, *data, defaultData);
                _detailPropertyContext.Draw_FloatProperty(
                    "샘플 간격",
                    data->sampleSpacing,
                    defaultData.sampleSpacing,
                    0.005f,
                    "이동 거리 기준으로 중간 trail sample을 삽입하는 간격입니다. 낮을수록 곡선이 촘촘해집니다."
                );
                _detailPropertyContext.Draw_UintProperty(
                    "곡선 분할",
                    data->curveSubdivision,
                    defaultData.curveSubdivision,
                    "한 frame 안에서 보강할 수 있는 최대 중간 sample 수입니다."
                );
                _detailPropertyContext.Draw_BoolProperty(
                    "탄젠트 스무딩",
                    data->smoothTangent,
                    defaultData.smoothTangent,
                    "앞뒤 sample을 함께 보아 base/tip 위치를 완만하게 평균 보정합니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
            ImGui::EndDisabled();
        }
        break;
    }
    case AuthoringTypeDataKind::Mesh:
    {
        const MeshTypeData* data = get_if<MeshTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("Mesh TypeData payload is invalid.");
            break;
        }

        if (Draw_MeshDataEntryButton(*data))
        {
            const Shared<Editor_Window> meshPreviewWindow = EDITOR != nullptr ? EDITOR->Get_Window(L"MeshData Preview") : nullptr;
            const Shared<MeshDataPreview_View> meshPreviewView = dynamic_pointer_cast<MeshDataPreview_View>(meshPreviewWindow);
            if (meshPreviewView != nullptr)
                meshPreviewView->Open_Target(emitter.id);
        }

        ImVec4 validationColor{};
        const string validationMessage = Build_MeshModelValidationMessage(*data, validationColor);
        if (validationMessage != "Model asset is valid for authoring.")
        {
            ImGui::PushStyleColor(ImGuiCol_Text, validationColor);
            ImGui::TextWrapped("%s", validationMessage.c_str());
            ImGui::PopStyleColor();
        }

        break;
    }
    case AuthoringTypeDataKind::Ribbon:
    {
        RibbonTypeData* data = get_if<RibbonTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("리본 TypeData payload is invalid.");
            break;
        }

        const RibbonTypeData defaultData{};
        if (ImGui::CollapsingHeader("리본", ImGuiTreeNodeFlags_DefaultOpen))
        {
            Draw_HistoryBudgetLink(emitterView, emitter);
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistoryRibbonSource"))
            {
                changed |= Draw_SourceHistoryRibbonSourceModeProperty(
                    _detailPropertyContext,
                    data->sourceMode,
                    defaultData.sourceMode
                );

                if (data->sourceMode == SourceHistoryRibbonSourceMode::SourceEmitter)
                {
                    data->sourceMode = data->sourceEmitterId != 0u
                                       ? SourceHistoryRibbonSourceMode::ParticleEmitter
                                       : SourceHistoryRibbonSourceMode::SelfRoot;
                    changed = true;
                }

                if (data->sourceMode == SourceHistoryRibbonSourceMode::SelfRoot && data->sourceEmitterId != 0u)
                {
                    data->sourceEmitterId = 0u;
                    changed = true;
                }
                else if (data->sourceEmitterId == emitter.id)
                {
                    data->sourceEmitterId = 0u;
                    changed = true;
                }

                if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                {
                    changed |= Draw_SourceHistoryRibbonSourceEmitterProperty(
                        _detailPropertyContext,
                        emitterView->Get_Emitters(),
                        emitter,
                        data->sourceEmitterId,
                        defaultData.sourceEmitterId
                    );
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "최대 추적 수",
                        data->followerLaneCount,
                        defaultData.followerLaneCount,
                        "동시에 따라갈 source particle 수 상한입니다."
                    );
                    data->followerLaneCount = max(1u, data->followerLaneCount);
                }

                _detailPropertyContext.End_PropertyTable();
            }

            if (_detailPropertyContext.Begin_PropertyTable("RibbonTypeData"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "꼬리 페이드 길이",
                    data->tailFadeLength,
                    defaultData.tailFadeLength,
                    0.01f,
                    "길이 제한 끝부분에서 alpha를 줄이는 거리입니다. 길이 제한이 0이면 적용되지 않습니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "꼬리 수축 사용",
                    data->tailCollapseOnIdle,
                    defaultData.tailCollapseOnIdle,
                    "켜면 source가 느리거나 멈췄을 때 기존 history sample을 현재 source 쪽으로 당깁니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "수축 속도",
                    data->tailCollapseSpeed,
                    defaultData.tailCollapseSpeed,
                    0.1f,
                    "source가 느리거나 멈췄을 때 tail sample을 현재 source 쪽으로 당기는 속도입니다."
                );
                data->tailCollapseSpeed = max(0.f, data->tailCollapseSpeed);
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "기본 폭",
                    data->baseWidth,
                    defaultData.baseWidth,
                    0.01f,
                    "SizeByLife를 곱하기 전 소스 히스토리 strip 기본 폭입니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }

            ImGui::TextDisabled("tail 유지 시간은 Lifetime 모듈에서 조정합니다. 빠른 source일수록 같은 시간 안에서 더 긴 tail이 남습니다.");
        }

        if (ImGui::CollapsingHeader("고급"))
        {
            if (ImGui::TreeNodeEx("히스토리 진단", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TextDisabled("비용성 history 기준값은 History Budget View에서 source group 단위로 확정합니다.");
                ImGui::BeginDisabled();
                if (_detailPropertyContext.Begin_PropertyTable("RibbonTypeDataCurveQuality"))
                {
                    Draw_RibbonCurveQualityProperty(_detailPropertyContext, *data, defaultData);
                    _detailPropertyContext.Draw_UintProperty(
                        "히스토리 수",
                        data->maxSampleCount,
                        defaultData.maxSampleCount,
                        "lane 하나가 보관할 최소 source history sample 수입니다. 필요하면 내부에서 더 크게 보정됩니다."
                    );
                    _detailPropertyContext.Draw_FloatProperty(
                        "샘플 간격",
                        data->sampleSpacing,
                        defaultData.sampleSpacing,
                        0.005f,
                        "source가 이 거리 이상 움직였을 때 새 history sample을 추가하고 곡선 sample 간격으로도 사용합니다."
                    );
                    _detailPropertyContext.Draw_FloatProperty(
                        "샘플 보강 간격",
                        data->sampleInterval,
                        defaultData.sampleInterval,
                        0.005f,
                        "0보다 크면 정지/저속에서도 시간 기준 sample을 보강합니다. projectile tail에서는 보통 0으로 둡니다."
                    );
                    _detailPropertyContext.Draw_UintProperty(
                        "곡선 분할",
                        data->curveSubdivision,
                        defaultData.curveSubdivision,
                        "source history segment 하나에서 보강할 수 있는 최대 곡선 render sample 수입니다."
                    );
                    _detailPropertyContext.Draw_BoolProperty(
                        "탄젠트 스무딩",
                        data->smoothTangent,
                        defaultData.smoothTangent,
                        "Custom 품질에서만 명시적으로 켭니다. 앞뒤 source sample을 함께 보아 path 위치를 완만하게 평균 보정합니다."
                    );
                    _detailPropertyContext.End_PropertyTable();
                }
                ImGui::EndDisabled();
                ImGui::TreePop();
            }

            if (_detailPropertyContext.Begin_PropertyTable("RibbonTypeDataAdvanced"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "길이 제한",
                    data->maxLength,
                    defaultData.maxLength,
                    0.01f,
                    "head 기준 최대 표시 길이입니다. tail 목표 길이가 아니라 과도한 순간 길이를 막는 상한입니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "생성 페이드",
                    data->laneSpawnFadeInEnabled,
                    defaultData.laneSpawnFadeInEnabled,
                    "켜면 새 follower lane의 시작 폭이 지정 시간 동안 0에서 원래 폭으로 커집니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "생성 페이드 시간",
                    data->laneSpawnFadeInDuration,
                    defaultData.laneSpawnFadeInDuration,
                    0.01f,
                    "새 follower lane 폭 fade-in 시간입니다. 위치 히스토리는 보정하지 않습니다."
                );
                data->laneSpawnFadeInDuration = max(0.f, data->laneSpawnFadeInDuration);
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "수명 페이드",
                    data->autoLifeFade,
                    defaultData.autoLifeFade,
                    "켜면 sample age에 따라 알파가 자동 감소합니다. tail을 직접 자르지 않고 기존 tail이 사라지는 과정을 보기 쉽게 만듭니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "타일링 거리",
                    data->tilingDistance,
                    defaultData.tilingDistance,
                    0.01f,
                    "0 이하면 전체 표시 길이에 맞춰 늘이고, 양수면 거리 기준으로 texture를 반복합니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
        }

        break;
    }
    case AuthoringTypeDataKind::SourceHistorySpriteTrail:
    {
        SourceHistorySpriteTrailTypeData* data = get_if<SourceHistorySpriteTrailTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("SourceHistorySpriteTrail TypeData payload is invalid.");
            break;
        }

        const SourceHistorySpriteTrailTypeData defaultData{};
        if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistorySpriteTrailSource"))
            {
                changed |= Draw_SourceHistoryRibbonSourceModeProperty(
                    _detailPropertyContext,
                    data->sourceMode,
                    defaultData.sourceMode
                );

                if (data->sourceMode == SourceHistoryRibbonSourceMode::SourceEmitter)
                {
                    data->sourceMode = data->sourceEmitterId != 0u
                                       ? SourceHistoryRibbonSourceMode::ParticleEmitter
                                       : SourceHistoryRibbonSourceMode::SelfRoot;
                    changed = true;
                }

                if (data->sourceMode == SourceHistoryRibbonSourceMode::SelfRoot && data->sourceEmitterId != 0u)
                {
                    data->sourceEmitterId = 0u;
                    changed = true;
                }
                else if (data->sourceEmitterId == emitter.id)
                {
                    data->sourceEmitterId = 0u;
                    changed = true;
                }

                if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                {
                    changed |= Draw_SourceHistoryRibbonSourceEmitterProperty(
                        _detailPropertyContext,
                        emitterView->Get_Emitters(),
                        emitter,
                        data->sourceEmitterId,
                        defaultData.sourceEmitterId
                    );
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "최대 추적 수",
                        data->followerLaneCount,
                        defaultData.followerLaneCount,
                        "동시에 따라갈 source particle lane 상한입니다."
                    );
                    data->followerLaneCount = max(1u, data->followerLaneCount);
                }

                _detailPropertyContext.End_PropertyTable();
            }

            if (_detailPropertyContext.Begin_PropertyTable("SourceHistorySpriteTrailVisualLength"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "길이 제한",
                    data->maxLength,
                    defaultData.maxLength,
                    0.01f,
                    "head 기준 stamp 생성 후보 path의 최대 길이입니다. 0 이하면 제한하지 않습니다."
                );
                data->maxLength = max(0.f, data->maxLength);
                _detailPropertyContext.End_PropertyTable();
            }
        }

        if (ImGui::CollapsingHeader("히스토리 진단"))
        {
            Draw_HistoryBudgetLink(emitterView, emitter);
            ImGui::TextDisabled("비용성 history 기준값은 History Budget View에서 source group 단위로 확정합니다.");
            ImGui::BeginDisabled();
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistorySpriteTrailHistory"))
            {
                _detailPropertyContext.Draw_FloatProperty(
                    "샘플 수명",
                    data->sampleLifetime,
                    defaultData.sampleLifetime,
                    0.01f,
                    "source history sample을 보관하는 시간입니다. stamp lifetime과 구분됩니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("곡선 진단"))
        {
            ImGui::TextDisabled("곡선 품질 기준값은 History Budget View에서 source group 단위로 확정합니다.");
            ImGui::BeginDisabled();
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistorySpriteTrailCurveQuality"))
            {
                Draw_SourceHistorySpriteTrailCurveQualityProperty(_detailPropertyContext, *data, defaultData);
                _detailPropertyContext.Draw_FloatProperty(
                    "샘플 간격",
                    data->sampleSpacing,
                    defaultData.sampleSpacing,
                    0.005f,
                    "source가 이 거리 이상 움직였을 때 새 history sample을 추가하고 곡선 path sample 간격으로도 사용합니다."
                );
                _detailPropertyContext.Draw_UintProperty(
                    "곡선 분할",
                    data->curveSubdivision,
                    defaultData.curveSubdivision,
                    "source history segment 하나에서 보강할 수 있는 최대 곡선 sample 수입니다."
                );
                _detailPropertyContext.Draw_BoolProperty(
                    "탄젠트 스무딩",
                    data->smoothTangent,
                    defaultData.smoothTangent,
                    "앞뒤 source sample을 함께 보아 stamp 위치와 path tangent를 완만하게 보정합니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Stamp", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistorySpriteTrailStamp"))
            {
                changed |= Draw_SourceHistorySpriteTrailStampSpawnModeProperty(
                    _detailPropertyContext,
                    data->stampSpawnMode,
                    defaultData.stampSpawnMode
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "스탬프 간격",
                    data->stampSpacing,
                    defaultData.stampSpacing,
                    0.005f,
                    "Distance 모드에서 path 위 stamp 사이 거리입니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "스탬프 시간 간격",
                    data->stampInterval,
                    defaultData.stampInterval,
                    0.005f,
                    "Time 모드에서 stamp를 찍는 시간 간격입니다."
                );
                changed |= _detailPropertyContext.Draw_UintProperty(
                    "최대 스탬프 수",
                    data->maxStampCount,
                    defaultData.maxStampCount,
                    "lane 하나가 보관할 stamp 상한입니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "지터",
                    data->spawnJitter,
                    defaultData.spawnJitter,
                    0.005f,
                    "path 주변 random offset 강도입니다. v0 기본값은 0입니다."
                );
                data->stampSpacing = max(0.001f, data->stampSpacing);
                data->stampInterval = max(0.001f, data->stampInterval);
                data->maxStampCount = max(1u, data->maxStampCount);
                data->spawnJitter = max(0.f, data->spawnJitter);
                _detailPropertyContext.End_PropertyTable();
            }
        }

        if (ImGui::CollapsingHeader("Card", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistorySpriteTrailCard"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "카드 길이",
                    data->cardLength,
                    defaultData.cardLength,
                    0.01f,
                    "stamp card의 기준 길이입니다. 화면 정렬 방식에 따라 path 방향 또는 카메라 평면에 배치됩니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "카드 폭",
                    data->cardWidth,
                    defaultData.cardWidth,
                    0.005f,
                    "stamp card의 기준 폭입니다. Square 화면 정렬에서는 길이/폭 중 큰 값으로 맞춥니다."
                );
                data->cardLength = max(0.001f, data->cardLength);
                data->cardWidth = max(0.001f, data->cardWidth);

                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "U 반전",
                    data->flipU,
                    defaultData.flipU,
                    "texture U 방향을 mirror 보정합니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "V 반전",
                    data->flipV,
                    defaultData.flipV,
                    "texture V 방향을 mirror 보정합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "회전 오프셋",
                    data->rotationOffsetDegrees,
                    defaultData.rotationOffsetDegrees,
                    1.f,
                    "authored texture의 기본 방향 차이를 degree 단위로 보정합니다."
                );

                _detailPropertyContext.End_PropertyTable();
            }
        }

        break;
    }
    case AuthoringTypeDataKind::Beam:
    {
        BeamTypeData* data = get_if<BeamTypeData>(&emitter.typeData.payload);
        if (nullptr == data)
        {
            ImGui::TextDisabled("Beam TypeData payload is invalid.");
            break;
        }

        const BeamTypeData defaultData{};
        if (ImGui::CollapsingHeader("Beam", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::CollapsingHeader("경로", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (_detailPropertyContext.Begin_PropertyTable("BeamTypeDataPath"))
                {
                    changed |= Draw_BeamEndpointModeProperty(_detailPropertyContext, data->endpointMode, defaultData.endpointMode);
                    changed |= _detailPropertyContext.Draw_Vec3Property(
                        "로컬 시작점",
                        data->localStart,
                        defaultData.localStart,
                        0.01f,
                        nullptr,
                        "Beam 시작점을 emitter local space 기준으로 지정합니다."
                    );
                    if (data->endpointMode == BeamEndpointMode::StartEnd)
                    {
                        changed |= _detailPropertyContext.Draw_Vec3Property(
                            "로컬 끝점",
                            data->localEnd,
                            defaultData.localEnd,
                            0.01f,
                            nullptr,
                            "시작/끝 방식에서 Beam 끝점을 emitter local space 기준으로 지정합니다."
                        );
                    }
                    else
                    {
                        changed |= _detailPropertyContext.Draw_Vec3Property(
                            "로컬 방향",
                            data->localDirection,
                            defaultData.localDirection,
                            0.01f,
                            nullptr,
                            "방향/길이 방식에서 Beam이 뻗을 emitter local 방향입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "길이",
                            data->length,
                            defaultData.length,
                            0.01f,
                            "방향 / 길이 방식에서 쓰는 Beam 길이입니다."
                        );
                    }
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "세그먼트 수",
                        data->segmentCount,
                        defaultData.segmentCount,
                        "생성되는 polyline segment 수입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "노이즈 강도",
                        data->noiseAmplitude,
                        defaultData.noiseAmplitude,
                        0.01f,
                        "중심선에서 벗어나는 deterministic jitter 크기입니다."
                    );
                    changed |= _detailPropertyContext.Draw_UintProperty("시드", data->seed, defaultData.seed, "생성 경로 jitter에 쓰는 seed입니다.");
                    _detailPropertyContext.End_PropertyTable();
                }
            }
            if (ImGui::CollapsingHeader("스트립 변화", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (_detailPropertyContext.Begin_PropertyTable("BeamTypeDataStripVariation"))
                {
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "스트립 수",
                        data->stripCount,
                        defaultData.stripCount,
                        "같은 시작점에서 생성할 strip 수입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "끝점 확산 반경",
                        data->endSpreadRadius,
                        defaultData.endSpreadRadius,
                        0.01f,
                        "clustered end가 기준 끝점 주변에 퍼지는 반경입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "길이 편차",
                        data->lengthVariance,
                        defaultData.lengthVariance,
                        0.01f,
                        "strip별 길이 차이를 만드는 deterministic 편차입니다."
                    );
                    _detailPropertyContext.End_PropertyTable();
                }
            }
            if (ImGui::CollapsingHeader("표현", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (_detailPropertyContext.Begin_PropertyTable("BeamTypeDataPresentation"))
                {
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "기본 폭",
                        data->baseWidth,
                        defaultData.baseWidth,
                        0.01f,
                        "SizeByLife를 곱하기 전 Beam 기본 폭입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "반복 거리",
                        data->tilingDistance,
                        defaultData.tilingDistance,
                        0.01f,
                        "0: 현재 Beam/가지 길이에 texture 전체를 한 번 늘려 붙입니다. 양수: 해당 월드 거리마다 texture를 반복하며, 짧으면 일부만 보입니다."
                    );
                    _detailPropertyContext.End_PropertyTable();
                }
            }
            if (ImGui::CollapsingHeader("가지", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (_detailPropertyContext.Begin_PropertyTable("BeamTypeDataBranch"))
                {
                    changed |= _detailPropertyContext.Draw_BoolProperty(
                        "사용",
                        data->branchEnabled,
                        defaultData.branchEnabled,
                        "main strip 중간에서 짧은 child branch를 생성합니다."
                    );
                    changed |= Draw_BeamBranchPresetProperty(
                        _detailPropertyContext,
                        data->branchPreset,
                        defaultData.branchPreset
                    );
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "개수",
                        data->branchCount,
                        defaultData.branchCount,
                        "strip별 최대 branch 수입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "확률",
                        data->branchChance,
                        defaultData.branchChance,
                        0.01f,
                        "branch 후보가 실제 생성될 확률입니다."
                    );
                    if (data->branchPreset == BeamBranchPreset::EndGuided)
                    {
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "끝점 퍼짐",
                            data->branchEndSpreadRadius,
                            defaultData.branchEndSpreadRadius,
                            0.01f,
                            "끝점 유도 가지의 목표점이 Beam 끝점 주변에 퍼지는 반경입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "벌어짐",
                            data->branchOutwardAmount,
                            defaultData.branchOutwardAmount,
                            0.01f,
                            "가지가 시작 직후 parent path에서 옆으로 벌어지는 정도입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "휘어짐",
                            data->branchCurveAmount,
                            defaultData.branchCurveAmount,
                            0.01f,
                            "옆으로 벌어진 가지가 끝점 목표로 다시 수렴하는 정도입니다."
                        );
                    }
                    else if (data->branchPreset == BeamBranchPreset::DownStrike)
                    {
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "내려꽂힘 길이",
                            data->branchDownLength,
                            defaultData.branchDownLength,
                            0.01f,
                            "가지가 emitter local down 방향으로 뻗는 길이입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "끝점 퍼짐",
                            data->branchEndSpreadRadius,
                            defaultData.branchEndSpreadRadius,
                            0.01f,
                            "내려꽂힘 가지의 끝점이 down axis 주변으로 퍼지는 반경입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "휘어짐",
                            data->branchCurveAmount,
                            defaultData.branchCurveAmount,
                            0.01f,
                            "가지가 down target으로 수렴하는 휘어짐 정도입니다."
                        );
                    }
                    else if (data->branchPreset == BeamBranchPreset::Entangle)
                    {
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "얽힘 반경",
                            data->branchEntangleRadius,
                            defaultData.branchEntangleRadius,
                            0.01f,
                            "가지가 parent path 주변을 감는 반경입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "진행 오프셋",
                            data->branchEntangleAdvance,
                            defaultData.branchEntangleAdvance,
                            0.01f,
                            "가지 target이 parent path를 따라 앞/뒤로 이동하는 비율입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "휘어짐",
                            data->branchCurveAmount,
                            defaultData.branchCurveAmount,
                            0.01f,
                            "얽힘 가지가 target으로 수렴하는 휘어짐 정도입니다."
                        );
                    }
                    else if (data->branchPreset == BeamBranchPreset::ShortCrack)
                    {
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "균열 길이",
                            data->branchCrackLength,
                            defaultData.branchCrackLength,
                            0.01f,
                            "가지가 start 주변에서 짧게 뻗는 길이입니다."
                        );
                        changed |= _detailPropertyContext.Draw_FloatProperty(
                            "균열 퍼짐",
                            data->branchCrackSpreadRadius,
                            defaultData.branchCrackSpreadRadius,
                            0.01f,
                            "짧은 균열 endpoint가 radial하게 퍼지는 반경입니다."
                        );
                    }
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "세그먼트 수",
                        data->branchSegmentCount,
                        defaultData.branchSegmentCount,
                        "child branch polyline segment 수입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "시작 최소",
                        data->branchStartMin,
                        defaultData.branchStartMin,
                        0.01f,
                        "parent path에서 branch 시작 구간의 최소 비율입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "시작 최대",
                        data->branchStartMax,
                        defaultData.branchStartMax,
                        0.01f,
                        "parent path에서 branch 시작 구간의 최대 비율입니다."
                    );
                    changed |= _detailPropertyContext.Draw_FloatProperty(
                        "폭 배율",
                        data->branchWidthScale,
                        defaultData.branchWidthScale,
                        0.01f,
                        "parent 폭 대비 branch 폭 배율입니다."
                    );
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "시드 오프셋",
                        data->branchSeedOffset,
                        defaultData.branchSeedOffset,
                        "parent seed와 섞어 branch 결과를 갈라놓는 salt입니다."
                    );
                    _detailPropertyContext.End_PropertyTable();
                }
            }
            data->length = max(0.f, data->length);
            data->segmentCount = max(1u, data->segmentCount);
            data->noiseAmplitude = max(0.f, data->noiseAmplitude);
            data->stripCount = max(1u, data->stripCount);
            data->endSpreadRadius = max(0.f, data->endSpreadRadius);
            data->lengthVariance = max(0.f, data->lengthVariance);
            data->branchChance = clamp(data->branchChance, 0.f, 1.f);
            data->branchSegmentCount = max(1u, data->branchSegmentCount);
            data->branchLength = max(0.f, data->branchLength);
            data->branchLengthVariance = max(0.f, data->branchLengthVariance);
            data->branchStartMin = clamp(data->branchStartMin, 0.f, 1.f);
            data->branchStartMax = clamp(data->branchStartMax, 0.f, 1.f);
            if (data->branchStartMin > data->branchStartMax)
                swap(data->branchStartMin, data->branchStartMax);
            data->branchSpreadRadius = max(0.f, data->branchSpreadRadius);
            data->branchEndSpreadRadius = max(0.f, data->branchEndSpreadRadius);
            data->branchOutwardAmount = max(0.f, data->branchOutwardAmount);
            data->branchCurveAmount = max(0.f, data->branchCurveAmount);
            data->branchDownLength = max(0.f, data->branchDownLength);
            data->branchEntangleRadius = max(0.f, data->branchEntangleRadius);
            data->branchEntangleAdvance = max(0.f, data->branchEntangleAdvance);
            data->branchCrackLength = max(0.f, data->branchCrackLength);
            data->branchCrackSpreadRadius = max(0.f, data->branchCrackSpreadRadius);
            data->branchWidthScale = max(0.f, data->branchWidthScale);
            data->baseWidth = max(0.001f, data->baseWidth);
            data->tilingDistance = max(0.f, data->tilingDistance);
        }
        break;
    }
    case AuthoringTypeDataKind::None:
    default:
        ImGui::TextDisabled("장착된 타입 데이터가 없습니다.");
        break;
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit TypeData Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_ModuleDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter,
    AuthoringModule& module)
{
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    bool changed = false;

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        if (auto* data = get_if<RequiredModuleData>(&module.data))
            changed = _requiredModuleDetail.Draw(emitter, module, *data, _detailPropertyContext, _detailTimeDelta);
        break;

    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
            changed = _spawnModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            changed = _lifetimeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            changed = _initialSizeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialMeshSize:
        if (auto* data = get_if<InitialMeshSizeModuleData>(&module.data))
            changed = _initialMeshSizeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialLocation:
        if (auto* data = get_if<InitialLocationModuleData>(&module.data))
            changed = _initialLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RibbonOrientation:
        if (auto* data = get_if<RibbonOrientationModuleData>(&module.data))
            changed = _ribbonOrientationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SphereLocation:
        if (auto* data = get_if<SphereLocationModuleData>(&module.data))
            changed = _sphereLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::PlaneRadialLocation:
        if (auto* data = get_if<PlaneRadialLocationModuleData>(&module.data))
            changed = _planeRadialLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::CylinderLocation:
        if (auto* data = get_if<CylinderLocationModuleData>(&module.data))
            changed = _cylinderLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialVelocity:
        if (auto* data = get_if<InitialVelocityModuleData>(&module.data))
            changed = _initialVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRadialVelocity:
        if (auto* data = get_if<InitialRadialVelocityModuleData>(&module.data))
            changed = _initialRadialVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::VelocityCone:
        if (auto* data = get_if<VelocityConeModuleData>(&module.data))
            changed = _velocityConeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SourceMotionVelocity:
        if (auto* data = get_if<SourceMotionVelocityModuleData>(&module.data))
            changed = _sourceMotionVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Acceleration:
        if (auto* data = get_if<AccelerationModuleData>(&module.data))
            changed = _accelerationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Drag:
        if (auto* data = get_if<DragModuleData>(&module.data))
            changed = _dragModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::VelocityOverLife:
        if (auto* data = get_if<VelocityOverLifeModuleData>(&module.data))
            changed = _velocityOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::OrbitOverLife:
        if (auto* data = get_if<OrbitOverLifeModuleData>(&module.data))
            changed = _orbitOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotation:
        if (auto* data = get_if<InitialRotationModuleData>(&module.data))
            changed = _initialRotationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SphereRadialOrientation:
        if (auto* data = get_if<SphereRadialOrientationModuleData>(&module.data))
            changed = _sphereRadialOrientationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::PlaneRadialOrientation:
        if (auto* data = get_if<PlaneRadialOrientationModuleData>(&module.data))
            changed = _planeRadialOrientationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::CylinderOrientation:
        if (auto* data = get_if<CylinderOrientationModuleData>(&module.data))
            changed = _cylinderOrientationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RotationOverLife:
        if (auto* data = get_if<RotationOverLifeModuleData>(&module.data))
            changed = _rotationOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SpriteTilt:
        if (auto* data = get_if<SpriteTiltModuleData>(&module.data))
            changed = _spriteTiltModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SpriteTiltOverLife:
        if (auto* data = get_if<SpriteTiltOverLifeModuleData>(&module.data))
            changed = _spriteTiltOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotationRate:
        if (auto* data = get_if<InitialRotationRateModuleData>(&module.data))
            changed = _initialRotationRateModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RotationRateScaleByLife:
        if (auto* data = get_if<RotationRateScaleByLifeModuleData>(&module.data))
            changed = _rotationRateScaleByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialMeshRotation:
        if (auto* data = get_if<InitialMeshRotationModuleData>(&module.data))
            changed = _initialMeshRotationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshRotationOverLife:
        if (auto* data = get_if<MeshRotationOverLifeModuleData>(&module.data))
            changed = _meshRotationOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshDirectionAlignOverLife:
        if (auto* data = get_if<MeshDirectionAlignOverLifeModuleData>(&module.data))
            changed = _meshDirectionAlignOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialMeshRotationRate:
        if (auto* data = get_if<InitialMeshRotationRateModuleData>(&module.data))
            changed = _initialMeshRotationRateModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshRotationRateScaleByLife:
        if (auto* data = get_if<MeshRotationRateScaleByLifeModuleData>(&module.data))
            changed = _meshRotationRateScaleByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialColor:
        if (auto* data = get_if<InitialColorModuleData>(&module.data))
            changed = _initialColorModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
            changed = _colorOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SubUVFrameOverLife:
        if (auto* data = get_if<SubUVFrameOverLifeModuleData>(&module.data))
            changed = _subUvFrameOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SizeByLife:
        if (auto* data = get_if<SizeByLifeModuleData>(&module.data))
            changed = _sizeByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::BeamEnvelopeOverLife:
        if (auto* data = get_if<BeamEnvelopeOverLifeModuleData>(&module.data))
            changed = _beamEnvelopeOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshSizeByLife:
        if (auto* data = get_if<MeshSizeByLifeModuleData>(&module.data))
            changed = _meshSizeByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SpawnPerUnit:
        if (auto* data = get_if<SpawnPerUnitModuleData>(&module.data))
            changed = _spawnPerUnitModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        if (auto* data = get_if<SourceHistorySpriteTrailPathFollowModuleData>(&module.data))
            changed = _sourceHistorySpriteTrailPathFollowModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        if (auto* data = get_if<SourceHistorySpriteTrailPathReplayModuleData>(&module.data))
            changed = _sourceHistorySpriteTrailPathReplayModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MaterialScalarModulation:
        if (auto* data = get_if<MaterialScalarModulationModuleData>(&module.data))
            changed = _materialScalarModulationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    default:
        ImGui::TextDisabled("Unsupported module type.");
        break;
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Module Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Mark_Changed(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, bool changed)
{
    if (!changed)
        return;

    emitter.previewDirty = true;
    if (emitterView)
        emitterView->MarkDirty();
    MarkDirty();
}

void Detail_View::Commit_PendingAuthoringEditOnTargetChange(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection)
{
    if (!_hasPendingAuthoringEdit || Is_SameSelection(_pendingAuthoringSelection, selection))
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void Detail_View::Begin_PendingAuthoringEdit(
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

void Detail_View::Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void Detail_View::Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || nullptr == emitterView)
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

bool Detail_View::Is_SameSelection(const EffectAuthoringSelection& lhs, const EffectAuthoringSelection& rhs)
{
    return
        lhs.kind == rhs.kind &&
        lhs.emitterId == rhs.emitterId &&
        lhs.moduleId == rhs.moduleId;
}

Shared<Detail_View> Detail_View::Create()
{
    return make_shared<Detail_View>();
}

void Detail_View::Free()
{
    __super::Free();
}

NS_END
