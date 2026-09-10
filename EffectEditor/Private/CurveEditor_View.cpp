#include "CurveEditor_View.h"

#include "EffectAuthoringValueRange.h"
#include "EffectEditorInstance.h"
#include "Emitter_View.h"
#include "Helper_String.h"
#include "Particle_Types.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr float kDefaultTrackListWidth{ 230.f };
    constexpr float kTrackListMinWidth{ 170.f };
    constexpr float kTrackListSplitterWidth{ 6.f };
    constexpr float kCurvePanelMinWidth{ 260.f };
    constexpr float kGraphHeight{ 260.f };
    constexpr float kGraphPadding{ 28.f };
    constexpr float kKeyHitRadius{ 8.f };
    constexpr float kMinimumDragValueRange{ 10.f };
    constexpr size_t kDistributionCurveMaxKeys{ kEffectDistributionCurveMaxKeys };

    const char* Get_InterpolationLabel(FloatCurveInterpolationMode mode)
    {
        switch (mode)
        {
        case FloatCurveInterpolationMode::Linear:
            return "Linear";
        case FloatCurveInterpolationMode::Constant:
            return "Constant";
        case FloatCurveInterpolationMode::CurveAutoClamped:
            return "Curve Auto Clamped";
        case FloatCurveInterpolationMode::CurveAuto:
            return "Curve Auto (미구현)";
        case FloatCurveInterpolationMode::CurveUser:
            return "Curve User (미구현)";
        case FloatCurveInterpolationMode::CurveBreak:
            return "Curve Break (미구현)";
        default:
            return "Linear";
        }
    }

    ImVec2 To_GraphPoint(const ImVec2& origin, const ImVec2& size, float time, float value, float minValue, float maxValue)
    {
        const float valueRange = max(maxValue - minValue, 0.0001f);
        const float normalizedValue = clamp((value - minValue) / valueRange, 0.f, 1.f);
        return ImVec2{
            origin.x + clamp(time, 0.f, 1.f) * size.x,
            origin.y + (1.f - normalizedValue) * size.y
        };
    }

    float From_GraphTime(const ImVec2& origin, const ImVec2& size, float x)
    {
        return clamp((x - origin.x) / max(size.x, 1.f), 0.f, 1.f);
    }

    float From_GraphValue(const ImVec2& origin, const ImVec2& size, float y, float minValue, float maxValue)
    {
        const float normalizedValue = 1.f - clamp((y - origin.y) / max(size.y, 1.f), 0.f, 1.f);
        return lerp(minValue, maxValue, normalizedValue);
    }

    float Get_DragValueUnitsPerPixel(float minValue, float maxValue, float graphHeight)
    {
        const float valueRange = max(maxValue - minValue, kMinimumDragValueRange);
        return valueRange / max(graphHeight, 1.f);
    }

    float Evaluate_CurveSegmentAutoClamped(const FloatCurveKeyData& left, const FloatCurveKeyData& right, float x)
    {
        const float width = max(0.0001f, right.time - left.time);
        const float t = clamp((x - left.time) / width, 0.f, 1.f);
        const float m0 = left.leaveTangent * width;
        const float m1 = right.arriveTangent * width;
        const float t2 = t * t;
        const float t3 = t2 * t;
        return (2.f * t3 - 3.f * t2 + 1.f) * left.value +
               (t3 - 2.f * t2 + t) * m0 +
               (-2.f * t3 + 3.f * t2) * right.value +
               (t3 - t2) * m1;
    }

    Vec2 Evaluate_CurveSegmentAutoClamped(const Vector2CurveKeyData& left, const Vector2CurveKeyData& right, float x)
    {
        const float width = max(0.0001f, right.time - left.time);
        const float t = clamp((x - left.time) / width, 0.f, 1.f);
        const Vec2 m0{ left.leaveTangent.x * width, left.leaveTangent.y * width };
        const Vec2 m1{ right.arriveTangent.x * width, right.arriveTangent.y * width };
        const float t2 = t * t;
        const float t3 = t2 * t;
        return Vec2{
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.x +
            (t3 - 2.f * t2 + t) * m0.x +
            (-2.f * t3 + 3.f * t2) * right.value.x +
            (t3 - t2) * m1.x,
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.y +
            (t3 - 2.f * t2 + t) * m0.y +
            (-2.f * t3 + 3.f * t2) * right.value.y +
            (t3 - t2) * m1.y
        };
    }

    Vec3 Evaluate_CurveSegmentAutoClamped(const Vector3CurveKeyData& left, const Vector3CurveKeyData& right, float x)
    {
        const float width = max(0.0001f, right.time - left.time);
        const float t = clamp((x - left.time) / width, 0.f, 1.f);
        const Vec3 m0{ left.leaveTangent.x * width, left.leaveTangent.y * width, left.leaveTangent.z * width };
        const Vec3 m1{ right.arriveTangent.x * width, right.arriveTangent.y * width, right.arriveTangent.z * width };
        const float t2 = t * t;
        const float t3 = t2 * t;
        return Vec3{
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.x +
            (t3 - 2.f * t2 + t) * m0.x +
            (-2.f * t3 + 3.f * t2) * right.value.x +
            (t3 - t2) * m1.x,
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.y +
            (t3 - 2.f * t2 + t) * m0.y +
            (-2.f * t3 + 3.f * t2) * right.value.y +
            (t3 - t2) * m1.y,
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.z +
            (t3 - 2.f * t2 + t) * m0.z +
            (-2.f * t3 + 3.f * t2) * right.value.z +
            (t3 - t2) * m1.z
        };
    }

    Color Evaluate_CurveSegmentAutoClamped(const ColorCurveKeyData& left, const ColorCurveKeyData& right, float x)
    {
        const float width = max(0.0001f, right.time - left.time);
        const float t = clamp((x - left.time) / width, 0.f, 1.f);
        const Color m0{ left.leaveTangent.x * width, left.leaveTangent.y * width, left.leaveTangent.z * width, 0.f };
        const Color m1{ right.arriveTangent.x * width, right.arriveTangent.y * width, right.arriveTangent.z * width, 0.f };
        const float t2 = t * t;
        const float t3 = t2 * t;
        return Color{
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.x +
            (t3 - 2.f * t2 + t) * m0.x +
            (-2.f * t3 + 3.f * t2) * right.value.x +
            (t3 - t2) * m1.x,
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.y +
            (t3 - 2.f * t2 + t) * m0.y +
            (-2.f * t3 + 3.f * t2) * right.value.y +
            (t3 - t2) * m1.y,
            (2.f * t3 - 3.f * t2 + 1.f) * left.value.z +
            (t3 - 2.f * t2 + t) * m0.z +
            (-2.f * t3 + 3.f * t2) * right.value.z +
            (t3 - t2) * m1.z,
            1.f
        };
    }

    FloatCurveInterpolationMode Get_SelectedKeyInterpolationMode(const vector<FloatCurveKeyData>& keys, optional<size_t> selectedKeyIndex)
    {
        return selectedKeyIndex.has_value() && *selectedKeyIndex < keys.size()
               ? keys[*selectedKeyIndex].interpolationMode
               : FloatCurveInterpolationMode::Linear;
    }

    FloatCurveInterpolationMode Get_SelectedKeyInterpolationMode(const vector<Vector2CurveKeyData>& keys, optional<size_t> selectedKeyIndex)
    {
        return selectedKeyIndex.has_value() && *selectedKeyIndex < keys.size()
               ? keys[*selectedKeyIndex].interpolationMode
               : FloatCurveInterpolationMode::Linear;
    }

    FloatCurveInterpolationMode Get_SelectedKeyInterpolationMode(const vector<Vector3CurveKeyData>& keys, optional<size_t> selectedKeyIndex)
    {
        return selectedKeyIndex.has_value() && *selectedKeyIndex < keys.size()
               ? keys[*selectedKeyIndex].interpolationMode
               : FloatCurveInterpolationMode::Linear;
    }

    FloatCurveInterpolationMode Get_SelectedKeyInterpolationMode(const vector<ColorCurveKeyData>& keys, optional<size_t> selectedKeyIndex)
    {
        return selectedKeyIndex.has_value() && *selectedKeyIndex < keys.size()
               ? keys[*selectedKeyIndex].interpolationMode
               : FloatCurveInterpolationMode::Linear;
    }

    template <typename TKey>
    const char* Get_CurveBulkInterpolationLabel(const vector<TKey>& keys)
    {
        if (keys.empty())
            return "Linear";

        const FloatCurveInterpolationMode firstMode = keys.front().interpolationMode;
        const bool allSameMode = ranges::all_of(
            keys,
            [firstMode](const TKey& key)
            {
                return key.interpolationMode == firstMode;
            }
        );
        return allSameMode ? Get_InterpolationLabel(firstMode) : "Mixed";
    }

    template <typename TKey>
    bool Apply_CurveBulkInterpolationMode(vector<TKey>& keys, FloatCurveInterpolationMode mode)
    {
        bool changed = false;
        for (TKey& key : keys)
        {
            if (key.interpolationMode == mode)
                continue;

            key.interpolationMode = mode;
            changed = true;
        }
        return changed;
    }
}

CurveEditor_View::CurveEditor_View()
    : Editor_Window{ L"Curve Editor", ICON_FA_CHART_LINE }
{
    _trackListWidth = kDefaultTrackListWidth;
}

void CurveEditor_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        const vector<CurveTarget> targets = Resolve_PinnedTargets();
        Remove_StalePinnedTracks(targets);
        Ensure_ActiveTrack(targets);

        if (targets.empty())
            Draw_EmptyState();
        else
        {
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const float maxTrackListWidth = max(
                kTrackListMinWidth,
                contentWidth - kTrackListSplitterWidth - kCurvePanelMinWidth
            );
            _trackListWidth = clamp(_trackListWidth, kTrackListMinWidth, maxTrackListWidth);

            Draw_TrackList(targets);
            ImGui::SameLine(0.f, 0.f);

            const ImVec2 splitterSize{
                kTrackListSplitterWidth,
                max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight())
            };
            ImGui::InvisibleButton("##CurveTrackListSplitter", splitterSize);
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                const ImU32 splitterColor = ImGui::IsItemHovered() || ImGui::IsItemActive()
                                            ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
                                            : ImGui::GetColorU32(ImGuiCol_Separator);
                drawList->AddLine(
                    ImVec2{ min.x + splitterSize.x * 0.5f, min.y },
                    ImVec2{ min.x + splitterSize.x * 0.5f, max.y },
                    splitterColor
                );
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ImGui::IsItemActive())
            {
                _trackListWidth = clamp(
                    _trackListWidth + ImGui::GetIO().MouseDelta.x,
                    kTrackListMinWidth,
                    maxTrackListWidth
                );
            }

            ImGui::SameLine(0.f, 0.f);

            // ReSharper disable once CppLocalVariableMayBeConst
            if (optional<CurveTarget> activeTarget = Get_ActiveTarget(targets))
            {
                ImGui::BeginGroup();
                Draw_TargetHeader(*activeTarget);
                Draw_CurveGraph(targets, *activeTarget);
                Draw_SelectedKeyInspector(*activeTarget);
                ImGui::EndGroup();
            }
        }
    }

    if (_hasPendingCurveEdit && !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        Commit_CurveAuthoringEdit();

    Set_Open(isOpen);
    ImGui::End();
}

void CurveEditor_View::Pin_ModuleTargets(uint32 emitterId, uint32 moduleId)
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView)
        return;

    AuthoringEmitter* emitter = emitterView->Find_Emitter(emitterId);
    AuthoringModule* module = emitterView->Find_Module(emitterId, moduleId);
    if (nullptr == emitter || nullptr == module)
        return;

    Clear_PinnedTracks();
    Pin_Targets(Collect_ModuleTargets(*emitter, *module));
}

bool CurveEditor_View::Convert_AndPin_ModuleTargets(uint32 emitterId, uint32 moduleId)
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView)
        return false;

    AuthoringEmitter* emitter = emitterView->Find_Emitter(emitterId);
    AuthoringModule* module = emitterView->Find_Module(emitterId, moduleId);
    if (nullptr == emitter || nullptr == module)
        return false;

    const vector<CurveTarget> targets = Collect_ModuleTargets(*emitter, *module, false);
    if (targets.empty())
        return false;

    Commit_CurveAuthoringEdit();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    vector<string> convertedProperties{};
    bool converted = false;
    for (const CurveTarget& target : targets)
    {
        if (ranges::find(convertedProperties, target.identity.propertyId) != convertedProperties.end())
            continue;

        convertedProperties.push_back(target.identity.propertyId);
        converted |= Convert_ActiveTargetToConstantCurve(target);
    }

    if (!converted)
        return false;

    emitter->previewDirty = true;
    emitterView->MarkDirty();
    MarkDirty();

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView->Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = beforeSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = beforeSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = beforeSnapshot.selectedModuleIndex;
    emitterView->Execute_AuthoringSnapshotCommand(beforeSnapshot, afterSnapshot, "Convert Module Curves");

    Clear_PinnedTracks();
    Pin_Targets(Collect_ModuleTargets(*emitter, *module));
    return true;
}

void CurveEditor_View::Clear_PinnedTracks()
{
    _pinnedTracks.clear();
    _activeTrack.reset();
    _selectedKeyIndex.reset();
}

bool CurveEditor_View::Is_ModuleFocused(uint32 emitterId, uint32 moduleId) const
{
    return ranges::any_of(
        _pinnedTracks,
        [emitterId, moduleId](const PinnedTrack& track)
        {
            return track.identity.emitterId == emitterId && track.identity.moduleId == moduleId;
        }
    );
}

bool CurveEditor_View::Has_ModuleCurveTargets(AuthoringEmitter& emitter, AuthoringModule& module)
{
    return !Collect_ModuleTargets(emitter, module).empty();
}

bool CurveEditor_View::Has_ModuleConvertibleCurveTargets(AuthoringEmitter& emitter, AuthoringModule& module)
{
    return !Collect_ModuleTargets(emitter, module, false).empty();
}

bool CurveEditor_View::Is_SameIdentity(const CurveTargetIdentity& lhs, const CurveTargetIdentity& rhs)
{
    return lhs == rhs;
}

const char* CurveEditor_View::Get_ChannelLabel(CurveTargetChannel channel)
{
    switch (channel)
    {
    case CurveTargetChannel::X:
        return "X";
    case CurveTargetChannel::Y:
        return "Y";
    case CurveTargetChannel::Z:
        return "Z";
    case CurveTargetChannel::R:
        return "R";
    case CurveTargetChannel::G:
        return "G";
    case CurveTargetChannel::B:
        return "B";
    case CurveTargetChannel::Value:
    default:
        return "Value";
    }
}

float CurveEditor_View::Read_Vector2Channel(const Vec2& value, CurveTargetChannel channel)
{
    return channel == CurveTargetChannel::Y ? value.y : value.x;
}

void CurveEditor_View::Write_Vector2Channel(Vec2& value, CurveTargetChannel channel, float channelValue)
{
    if (channel == CurveTargetChannel::Y)
        value.y = channelValue;
    else
        value.x = channelValue;
}

float CurveEditor_View::Read_Vector3Channel(const Vec3& value, CurveTargetChannel channel)
{
    if (channel == CurveTargetChannel::Z)
        return value.z;
    return channel == CurveTargetChannel::Y ? value.y : value.x;
}

void CurveEditor_View::Write_Vector3Channel(Vec3& value, CurveTargetChannel channel, float channelValue)
{
    if (channel == CurveTargetChannel::Z)
        value.z = channelValue;
    else if (channel == CurveTargetChannel::Y)
        value.y = channelValue;
    else
        value.x = channelValue;
}

float CurveEditor_View::Read_ColorChannel(const Color& value, CurveTargetChannel channel)
{
    if (channel == CurveTargetChannel::B)
        return value.z;
    return channel == CurveTargetChannel::G ? value.y : value.x;
}

void CurveEditor_View::Write_ColorChannel(Color& value, CurveTargetChannel channel, float channelValue)
{
    if (channel == CurveTargetChannel::B)
        value.z = clamp(channelValue, 0.f, 1.f);
    else if (channel == CurveTargetChannel::G)
        value.y = clamp(channelValue, 0.f, 1.f);
    else
        value.x = clamp(channelValue, 0.f, 1.f);
    value.w = 1.f;
}

string CurveEditor_View::Build_Label(const AuthoringEmitter& emitter, const AuthoringModule& module, const string& propertyLabel, CurveTargetChannel channel)
{
    string label = emitter.name + " / " + String::ToString(module.displayName) + " / " + propertyLabel;
    if (channel != CurveTargetChannel::Value)
        label += string{ " " } + Get_ChannelLabel(channel);
    return label;
}

ImU32 CurveEditor_View::Get_TargetColor(const CurveTarget& target)
{
    if (target.channel == CurveTargetChannel::X)
        return IM_COL32(235, 85, 85, 255);

    if (target.channel == CurveTargetChannel::Y)
        return IM_COL32(85, 210, 125, 255);

    if (target.channel == CurveTargetChannel::Z)
        return IM_COL32(95, 145, 245, 255);

    if (target.channel == CurveTargetChannel::R)
        return IM_COL32(235, 85, 85, 255);

    if (target.channel == CurveTargetChannel::G)
        return IM_COL32(85, 210, 125, 255);

    if (target.channel == CurveTargetChannel::B)
        return IM_COL32(95, 145, 245, 255);

    if (target.identity.propertyId.find("alpha") != string::npos || target.identity.propertyId.find("Alpha") != string::npos)
        return IM_COL32(220, 190, 255, 255);

    return target.color != 0 ? target.color : IM_COL32(245, 205, 95, 255);
}

static const char* Get_MaterialScalarTargetLabel(MaterialScalarModulationTargetField target)
{
    switch (target)
    {
    case MaterialScalarModulationTargetField::Intensity:
        return "Intensity";
    case MaterialScalarModulationTargetField::OpacityPower:
        return "OpacityPower";
    case MaterialScalarModulationTargetField::NoiseStrength:
        return "NoiseStrength";
    case MaterialScalarModulationTargetField::AlphaErosion:
        return "AlphaErosion";
    case MaterialScalarModulationTargetField::AlphaCutoff:
        return "AlphaCutoff";
    case MaterialScalarModulationTargetField::AlphaMultiplier:
        return "AlphaMultiplier";
    case MaterialScalarModulationTargetField::CoreIntensity:
        return "CoreIntensity";
    case MaterialScalarModulationTargetField::OuterIntensity:
        return "OuterIntensity";
    case MaterialScalarModulationTargetField::CoreColor:
        return "CoreColor";
    case MaterialScalarModulationTargetField::RefractionIntensity:
        return "Refraction Intensity";
    case MaterialScalarModulationTargetField::MainUVScrollSpeedScale:
        return "Main UV Scroll Speed Scale";
    case MaterialScalarModulationTargetField::NoiseUVScrollSpeedScale:
        return "Noise UV Scroll Speed Scale";
    case MaterialScalarModulationTargetField::MaskUVScrollSpeedScale:
        return "Mask UV Scroll Speed Scale";
    case MaterialScalarModulationTargetField::FlowUVScrollSpeedScale:
        return "Flow UV Scroll Speed Scale";
    default:
        return "Intensity";
    }
}

static const char* Get_MaterialVec2TargetLabel(MaterialVec2ModulationTargetField target)
{
    switch (target)
    {
    case MaterialVec2ModulationTargetField::NoiseUVOffset:
        return "Noise UV Offset";
    case MaterialVec2ModulationTargetField::MaskUVOffset:
        return "Mask UV Offset";
    case MaterialVec2ModulationTargetField::FlowUVOffset:
        return "Flow UV Offset";
    case MaterialVec2ModulationTargetField::MainUVOffset:
    default:
        return "Main UV Offset";
    }
}

float CurveEditor_View::Get_TargetValue(const CurveTarget& target, const FloatCurveKeyData& key)
{
    return target.payloadKind == CurveTargetPayloadKind::Float ? key.value : 0.f;
}

float CurveEditor_View::Get_TargetValue(const CurveTarget& target, const Vector2CurveKeyData& key)
{
    return target.payloadKind == CurveTargetPayloadKind::Vector2 ? Read_Vector2Channel(key.value, target.channel) : 0.f;
}

float CurveEditor_View::Get_TargetValue(const CurveTarget& target, const Vector3CurveKeyData& key)
{
    return target.payloadKind == CurveTargetPayloadKind::Vector3 ? Read_Vector3Channel(key.value, target.channel) : 0.f;
}

float CurveEditor_View::Get_TargetValue(const CurveTarget& target, const ColorCurveKeyData& key)
{
    return target.payloadKind == CurveTargetPayloadKind::Color ? Read_ColorChannel(key.value, target.channel) : 0.f;
}

void CurveEditor_View::Set_TargetValue(const CurveTarget& target, FloatCurveKeyData& key, float value)
{
    if (target.payloadKind == CurveTargetPayloadKind::Float)
        key.value = value;
}

void CurveEditor_View::Set_TargetValue(const CurveTarget& target, Vector2CurveKeyData& key, float value)
{
    if (target.payloadKind == CurveTargetPayloadKind::Vector2)
        Write_Vector2Channel(key.value, target.channel, value);
}

void CurveEditor_View::Set_TargetValue(const CurveTarget& target, Vector3CurveKeyData& key, float value)
{
    if (target.payloadKind == CurveTargetPayloadKind::Vector3)
        Write_Vector3Channel(key.value, target.channel, value);
}

void CurveEditor_View::Set_TargetValue(const CurveTarget& target, ColorCurveKeyData& key, float value)
{
    if (target.payloadKind == CurveTargetPayloadKind::Color)
        Write_ColorChannel(key.value, target.channel, value);
}

float CurveEditor_View::Get_DefaultValue(const FloatDistributionData& distribution)
{
    if (const auto* constantData = get_if<ConstantFloatDistributionData>(&distribution.payload))
        return constantData->value;

    if (const auto* uniformData = get_if<UniformFloatDistributionData>(&distribution.payload))
        return uniformData->maxValue;

    if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
        nullptr != curve && !curve->keys.empty())
        return curve->keys.front().value;

    return 0.f;
}

Vec2 CurveEditor_View::Get_DefaultValue(const Vector2DistributionData& distribution)
{
    if (const auto* constantData = get_if<ConstantVector2DistributionData>(&distribution.payload))
        return constantData->value;

    if (const auto* uniformData = get_if<UniformVector2DistributionData>(&distribution.payload))
        return uniformData->maxValue;

    if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&distribution.payload);
        nullptr != curve && !curve->keys.empty())
        return curve->keys.front().value;

    return Vec2{ 1.f, 1.f };
}

Vec3 CurveEditor_View::Get_DefaultValue(const Vector3DistributionData& distribution)
{
    if (const auto* constantData = get_if<ConstantVector3DistributionData>(&distribution.payload))
        return constantData->value;

    if (const auto* uniformData = get_if<UniformVector3DistributionData>(&distribution.payload))
        return uniformData->maxValue;

    if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&distribution.payload);
        nullptr != curve && !curve->keys.empty())
        return curve->keys.front().value;

    return Vec3{ 0.f, 0.f, 0.f };
}

Color CurveEditor_View::Get_DefaultValue(const ColorDistributionData& distribution)
{
    if (const auto* constantData = get_if<ConstantColorDistributionData>(&distribution.payload))
        return constantData->value;

    if (const auto* uniformData = get_if<UniformColorDistributionData>(&distribution.payload))
        return uniformData->maxValue;

    if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&distribution.payload);
        nullptr != curve && !curve->keys.empty())
        return curve->keys.front().value;

    return Color{ 1.f, 1.f, 1.f, 1.f };
}

ConstantCurveFloatDistributionData* CurveEditor_View::Resolve_CurvePayload(FloatDistributionData& distribution)
{
    if (distribution.mode != DistributionMode::ConstantCurve)
        return nullptr;

    return get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
}

ConstantCurveVector2DistributionData* CurveEditor_View::Resolve_CurvePayload(Vector2DistributionData& distribution)
{
    if (distribution.mode != DistributionMode::ConstantCurve)
        return nullptr;

    return get_if<ConstantCurveVector2DistributionData>(&distribution.payload);
}

ConstantCurveVector3DistributionData* CurveEditor_View::Resolve_CurvePayload(Vector3DistributionData& distribution)
{
    if (distribution.mode != DistributionMode::ConstantCurve)
        return nullptr;

    return get_if<ConstantCurveVector3DistributionData>(&distribution.payload);
}

ConstantCurveColorDistributionData* CurveEditor_View::Resolve_CurvePayload(ColorDistributionData& distribution)
{
    if (distribution.mode != DistributionMode::ConstantCurve)
        return nullptr;

    return get_if<ConstantCurveColorDistributionData>(&distribution.payload);
}

void CurveEditor_View::Normalize_CurveKeys(ConstantCurveFloatDistributionData& curve)
{
    if (curve.keys.empty())
        curve.keys = get<ConstantCurveFloatDistributionData>(FloatDistributionData::Make_ConstantCurve(0.f).payload).keys;

    ranges::sort(curve.keys, {}, &FloatCurveKeyData::time);
    for (FloatCurveKeyData& key : curve.keys)
        key.time = clamp(key.time, 0.f, 1.f);

    for (size_t index = 0; index < curve.keys.size(); ++index)
    {
        FloatCurveKeyData& key = curve.keys[index];
        if (key.interpolationMode != FloatCurveInterpolationMode::CurveAutoClamped)
            continue;

        const FloatCurveKeyData* prevKey = index > 0
                                           ? &curve.keys[index - 1]
                                           : nullptr;
        const FloatCurveKeyData* nextKey = index + 1 < curve.keys.size()
                                           ? &curve.keys[index + 1]
                                           : nullptr;
        const float dt = nullptr != prevKey && nullptr != nextKey
                         ? nextKey->time - prevKey->time
                         : 0.f;
        const float tangent = dt > 0.f
                              ? (nextKey->value - prevKey->value) / dt
                              : 0.f;
        key.arriveTangent = tangent;
        key.leaveTangent = tangent;
    }
}

void CurveEditor_View::Normalize_CurveKeys(ConstantCurveVector2DistributionData& curve)
{
    if (curve.keys.empty())
    {
        curve.keys = get<ConstantCurveVector2DistributionData>(
            Vector2DistributionData::Make_ConstantCurve(Vec2{ 1.f, 1.f }).payload
        ).keys;
    }

    ranges::sort(curve.keys, {}, &Vector2CurveKeyData::time);
    for (Vector2CurveKeyData& key : curve.keys)
        key.time = clamp(key.time, 0.f, 1.f);

    for (size_t index = 0; index < curve.keys.size(); ++index)
    {
        Vector2CurveKeyData& key = curve.keys[index];
        if (key.interpolationMode != FloatCurveInterpolationMode::CurveAutoClamped)
            continue;

        const Vector2CurveKeyData* prevKey = index > 0 ? &curve.keys[index - 1] : nullptr;
        const Vector2CurveKeyData* nextKey = index + 1 < curve.keys.size() ? &curve.keys[index + 1] : nullptr;
        const float dt = nullptr != prevKey && nullptr != nextKey ? nextKey->time - prevKey->time : 0.f;
        const Vec2 tangent = dt > 0.f
                             ? Vec2{ (nextKey->value.x - prevKey->value.x) / dt, (nextKey->value.y - prevKey->value.y) / dt }
                             : Vec2{};
        key.arriveTangent = tangent;
        key.leaveTangent = tangent;
    }
}

void CurveEditor_View::Normalize_CurveKeys(ConstantCurveVector3DistributionData& curve)
{
    if (curve.keys.empty())
    {
        curve.keys = get<ConstantCurveVector3DistributionData>(
            Vector3DistributionData::Make_ConstantCurve(Vec3{ 0.f, 0.f, 0.f }).payload
        ).keys;
    }

    ranges::sort(curve.keys, {}, &Vector3CurveKeyData::time);
    for (Vector3CurveKeyData& key : curve.keys)
        key.time = clamp(key.time, 0.f, 1.f);

    for (size_t index = 0; index < curve.keys.size(); ++index)
    {
        Vector3CurveKeyData& key = curve.keys[index];
        if (key.interpolationMode != FloatCurveInterpolationMode::CurveAutoClamped)
            continue;

        const Vector3CurveKeyData* prevKey = index > 0 ? &curve.keys[index - 1] : nullptr;
        const Vector3CurveKeyData* nextKey = index + 1 < curve.keys.size() ? &curve.keys[index + 1] : nullptr;
        const float dt = nullptr != prevKey && nullptr != nextKey ? nextKey->time - prevKey->time : 0.f;
        const Vec3 tangent = dt > 0.f
                             ? Vec3{
                                 (nextKey->value.x - prevKey->value.x) / dt,
                                 (nextKey->value.y - prevKey->value.y) / dt,
                                 (nextKey->value.z - prevKey->value.z) / dt }
                             : Vec3{};
        key.arriveTangent = tangent;
        key.leaveTangent = tangent;
    }
}

void CurveEditor_View::Normalize_CurveKeys(ConstantCurveColorDistributionData& curve)
{
    if (curve.keys.empty())
    {
        curve.keys = get<ConstantCurveColorDistributionData>(
            ColorDistributionData::Make_ConstantCurve(Color{ 1.f, 1.f, 1.f, 1.f }).payload
        ).keys;
    }

    ranges::sort(curve.keys, {}, &ColorCurveKeyData::time);
    for (ColorCurveKeyData& key : curve.keys)
    {
        key.time = clamp(key.time, 0.f, 1.f);
        key.value.w = 1.f;
        key.arriveTangent.w = 0.f;
        key.leaveTangent.w = 0.f;
    }

    for (size_t index = 0; index < curve.keys.size(); ++index)
    {
        ColorCurveKeyData& key = curve.keys[index];
        if (key.interpolationMode != FloatCurveInterpolationMode::CurveAutoClamped)
            continue;

        const ColorCurveKeyData* prevKey = index > 0 ? &curve.keys[index - 1] : nullptr;
        const ColorCurveKeyData* nextKey = index + 1 < curve.keys.size() ? &curve.keys[index + 1] : nullptr;
        const float dt = nullptr != prevKey && nullptr != nextKey ? nextKey->time - prevKey->time : 0.f;
        const Color tangent = dt > 0.f
                              ? Color{
                                  (nextKey->value.x - prevKey->value.x) / dt,
                                  (nextKey->value.y - prevKey->value.y) / dt,
                                  (nextKey->value.z - prevKey->value.z) / dt,
                                  0.f
                              }
                              : Color{};
        key.arriveTangent = tangent;
        key.leaveTangent = tangent;
    }
}

float CurveEditor_View::Evaluate_Curve(const ConstantCurveFloatDistributionData& curve, float x)
{
    if (curve.keys.empty())
        return 0.f;

    if (x <= curve.keys.front().time)
        return curve.keys.front().value;

    for (size_t index = 0; index + 1 < curve.keys.size(); ++index)
    {
        const FloatCurveKeyData& left = curve.keys[index];
        const FloatCurveKeyData& right = curve.keys[index + 1];
        if (x > right.time)
            continue;

        if (left.interpolationMode == FloatCurveInterpolationMode::Constant)
            return left.value;

        if (left.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            return Evaluate_CurveSegmentAutoClamped(left, right, x);

        const float span = max(right.time - left.time, 0.0001f);
        const float ratio = clamp((x - left.time) / span, 0.f, 1.f);
        return lerp(left.value, right.value, ratio);
    }

    return curve.keys.back().value;
}

Vec2 CurveEditor_View::Evaluate_Curve(const ConstantCurveVector2DistributionData& curve, float x)
{
    if (curve.keys.empty())
        return Vec2{ 1.f, 1.f };

    if (x <= curve.keys.front().time)
        return curve.keys.front().value;

    for (size_t index = 0; index + 1 < curve.keys.size(); ++index)
    {
        const Vector2CurveKeyData& left = curve.keys[index];
        const Vector2CurveKeyData& right = curve.keys[index + 1];
        if (x > right.time)
            continue;

        if (left.interpolationMode == FloatCurveInterpolationMode::Constant)
            return left.value;

        if (left.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            return Evaluate_CurveSegmentAutoClamped(left, right, x);

        const float span = max(right.time - left.time, 0.0001f);
        const float ratio = clamp((x - left.time) / span, 0.f, 1.f);
        return Vec2{
            lerp(left.value.x, right.value.x, ratio),
            lerp(left.value.y, right.value.y, ratio)
        };
    }

    return curve.keys.back().value;
}

Vec3 CurveEditor_View::Evaluate_Curve(const ConstantCurveVector3DistributionData& curve, float x)
{
    if (curve.keys.empty())
        return Vec3{ 0.f, 0.f, 0.f };

    if (x <= curve.keys.front().time)
        return curve.keys.front().value;

    for (size_t index = 0; index + 1 < curve.keys.size(); ++index)
    {
        const Vector3CurveKeyData& left = curve.keys[index];
        const Vector3CurveKeyData& right = curve.keys[index + 1];
        if (x > right.time)
            continue;

        if (left.interpolationMode == FloatCurveInterpolationMode::Constant)
            return left.value;

        if (left.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            return Evaluate_CurveSegmentAutoClamped(left, right, x);

        const float span = max(right.time - left.time, 0.0001f);
        const float ratio = clamp((x - left.time) / span, 0.f, 1.f);
        return Vec3{
            lerp(left.value.x, right.value.x, ratio),
            lerp(left.value.y, right.value.y, ratio),
            lerp(left.value.z, right.value.z, ratio)
        };
    }

    return curve.keys.back().value;
}

Color CurveEditor_View::Evaluate_Curve(const ConstantCurveColorDistributionData& curve, float x)
{
    if (curve.keys.empty())
        return Color{ 1.f, 1.f, 1.f, 1.f };

    if (x <= curve.keys.front().time)
        return curve.keys.front().value;

    for (size_t index = 0; index + 1 < curve.keys.size(); ++index)
    {
        const ColorCurveKeyData& left = curve.keys[index];
        const ColorCurveKeyData& right = curve.keys[index + 1];
        if (x > right.time)
            continue;

        if (left.interpolationMode == FloatCurveInterpolationMode::Constant)
            return left.value;

        if (left.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            return Evaluate_CurveSegmentAutoClamped(left, right, x);

        const float span = max(right.time - left.time, 0.0001f);
        const float ratio = clamp((x - left.time) / span, 0.f, 1.f);
        return Color{
            lerp(left.value.x, right.value.x, ratio),
            lerp(left.value.y, right.value.y, ratio),
            lerp(left.value.z, right.value.z, ratio),
            1.f
        };
    }

    return curve.keys.back().value;
}

CurveEditor_View::TargetEditRange CurveEditor_View::Compute_TargetEditRange(const CurveTarget& target)
{
    TargetEditRange editRange{};
    if (target.payloadKind == CurveTargetPayloadKind::Color)
        editRange = TargetEditRange{ 0.f, 1.f };

    if (const AuthoringValueRange* range = Find_AuthoringValueRange(target.identity.moduleType, target.identity.propertyId))
    {
        if (range->hasMin)
            editRange.minValue = range->minValue;
        if (range->hasMax)
            editRange.maxValue = range->maxValue;
    }

    return editRange;
}

float CurveEditor_View::Clamp_GraphEditedValue(const CurveTarget& target, float value)
{
    const TargetEditRange editRange = Compute_TargetEditRange(target);
    return clamp(value, editRange.minValue, editRange.maxValue);
}

Shared<Emitter_View> CurveEditor_View::Resolve_EmitterView() const
{
    if (nullptr == EDITOR)
        return nullptr;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    return dynamic_pointer_cast<Emitter_View>(emitterWindow);
}

vector<CurveEditor_View::CurveTarget> CurveEditor_View::Collect_ModuleTargets(
    AuthoringEmitter& emitter,
    AuthoringModule& module,
    bool requireActiveCurve)
{
    vector<CurveTarget> targets{};

    switch (module.type)
    {
    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
        {
            Add_FloatTarget(targets, emitter, module, data->spawnRate, "spawnRate", "Spawn Rate", IM_COL32(245, 205, 95, 255), requireActiveCurve);
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->spawnRateScale,
                "spawnRateScale",
                "Spawn Rate Scale",
                IM_COL32(245, 205, 95, 255),
                requireActiveCurve
            );
            Add_FloatTarget(targets, emitter, module, data->burstScale, "burstScale", "Burst Scale", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        }
        break;
    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->lifeTime, "lifeTime", "Life Time", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            Add_Vector2Target(targets, emitter, module, data->size, "size", "Initial Size", requireActiveCurve);
        break;
    case AuthoringModuleType::InitialMeshSize:
        if (auto* data = get_if<InitialMeshSizeModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->size, "size", "Initial Mesh Size", true, requireActiveCurve);
        break;
    case AuthoringModuleType::InitialLocation:
        if (auto* data = get_if<InitialLocationModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->location, "location", "Initial Location", false, requireActiveCurve);
        break;
    case AuthoringModuleType::InitialVelocity:
        if (auto* data = get_if<InitialVelocityModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->velocity, "velocity", "Initial Velocity", true, requireActiveCurve);
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        if (auto* data = get_if<InitialRadialVelocityModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->speed, "speed", "Speed", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::VelocityCone:
        if (auto* data = get_if<VelocityConeModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->speed, "speed", "Cone Speed", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::SourceMotionVelocity:
        if (auto* data = get_if<SourceMotionVelocityModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->speed, "speed", "Source Motion Speed", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::Drag:
        if (auto* data = get_if<DragModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->drag, "drag", "Drag", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::VelocityOverLife:
        if (auto* data = get_if<VelocityOverLifeModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->scaleOverLife, "scaleOverLife", "Velocity Scale", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::OrbitOverLife:
        if (auto* data = get_if<OrbitOverLifeModuleData>(&module.data))
        {
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->angleDegreesOverLife,
                "angleDegreesOverLife",
                "Orbit Angle",
                IM_COL32(120, 210, 245, 255),
                requireActiveCurve
            );
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->radiusScaleOverLife,
                "radiusScaleOverLife",
                "Orbit Radius Scale",
                IM_COL32(120, 245, 180, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::Acceleration:
        if (auto* data = get_if<AccelerationModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->acceleration, "acceleration", "Acceleration", true, requireActiveCurve);
        break;
    case AuthoringModuleType::InitialRotation:
        if (auto* data = get_if<InitialRotationModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->rotationDegrees, "rotationDegrees", "Rotation", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::RotationOverLife:
        if (auto* data = get_if<RotationOverLifeModuleData>(&module.data))
        {
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->rotationOverLife,
                "rotationOverLife",
                "Rotation Over Life",
                IM_COL32(245, 205, 95, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::InitialRotationRate:
        if (auto* data = get_if<InitialRotationRateModuleData>(&module.data))
        {
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->rotationRateDegrees,
                "rotationRateDegrees",
                "Rotation Rate",
                IM_COL32(245, 205, 95, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::RotationRateScaleByLife:
        if (auto* data = get_if<RotationRateScaleByLifeModuleData>(&module.data))
        {
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->scaleOverLife,
                "scaleOverLife",
                "Rotation Rate Scale",
                IM_COL32(245, 205, 95, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::SpriteTilt:
        if (auto* data = get_if<SpriteTiltModuleData>(&module.data))
            Add_Vector2Target(targets, emitter, module, data->tiltDegrees, "tiltDegrees", "Sprite Tilt", requireActiveCurve);
        break;
    case AuthoringModuleType::SpriteTiltOverLife:
        if (auto* data = get_if<SpriteTiltOverLifeModuleData>(&module.data))
            Add_Vector2Target(targets, emitter, module, data->tiltOverLife, "tiltOverLife", "Sprite Tilt Over Life", requireActiveCurve);
        break;
    case AuthoringModuleType::InitialMeshRotation:
        if (auto* data = get_if<InitialMeshRotationModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->rotationDegrees, "rotationDegrees", "Mesh Rotation", true, requireActiveCurve);
        break;
    case AuthoringModuleType::MeshRotationOverLife:
        if (auto* data = get_if<MeshRotationOverLifeModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->rotationOverLife, "rotationOverLife", "Mesh Rotation Over Life", true, requireActiveCurve);
        break;
    case AuthoringModuleType::MeshDirectionAlignOverLife:
        if (auto* data = get_if<MeshDirectionAlignOverLifeModuleData>(&module.data))
        {
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->alignmentProgress,
                "alignmentProgress",
                "Mesh Direction Align Progress",
                IM_COL32(125, 205, 255, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::InitialMeshRotationRate:
        if (auto* data = get_if<InitialMeshRotationRateModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->rotationRateDegrees, "rotationRateDegrees", "Mesh Rotation Rate", true, requireActiveCurve);
        break;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        if (auto* data = get_if<MeshRotationRateScaleByLifeModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->scaleOverLife, "scaleOverLife", "Mesh Rotation Rate Scale", true, requireActiveCurve);
        break;
    case AuthoringModuleType::InitialColor:
        if (auto* data = get_if<InitialColorModuleData>(&module.data))
        {
            Add_ColorTarget(targets, emitter, module, data->color, "color", "Color", requireActiveCurve);
            Add_FloatTarget(targets, emitter, module, data->alpha, "alpha", "Alpha", IM_COL32(220, 190, 255, 255), requireActiveCurve);
        }
        break;
    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
        {
            Add_ColorTarget(targets, emitter, module, data->colorOverLife, "colorOverLife", "Color Over Life", requireActiveCurve);
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->alphaOverLife,
                "alphaOverLife",
                "Alpha Over Life",
                IM_COL32(220, 190, 255, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::SizeByLife:
        if (auto* data = get_if<SizeByLifeModuleData>(&module.data))
            Add_Vector2Target(targets, emitter, module, data->scaleOverLife, "scaleOverLife", "Scale Over Life", requireActiveCurve);
        break;
    case AuthoringModuleType::BeamEnvelopeOverLife:
        if (auto* data = get_if<BeamEnvelopeOverLifeModuleData>(&module.data))
        {
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->startRatioOverLife,
                "startRatioOverLife",
                "Beam Start Ratio",
                IM_COL32(125, 205, 255, 255),
                requireActiveCurve
            );
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->endRatioOverLife,
                "endRatioOverLife",
                "Beam End Ratio",
                IM_COL32(255, 205, 125, 255),
                requireActiveCurve
            );
            Add_FloatTarget(
                targets,
                emitter,
                module,
                data->widthScaleOverLife,
                "widthScaleOverLife",
                "Beam Width Scale",
                IM_COL32(190, 255, 155, 255),
                requireActiveCurve
            );
        }
        break;
    case AuthoringModuleType::MeshSizeByLife:
        if (auto* data = get_if<MeshSizeByLifeModuleData>(&module.data))
            Add_Vector3Target(targets, emitter, module, data->scaleOverLife, "scaleOverLife", "Mesh Scale Over Life", true, requireActiveCurve);
        break;
    case AuthoringModuleType::SpawnPerUnit:
        if (auto* data = get_if<SpawnPerUnitModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->spawnPerUnit, "spawnPerUnit", "Spawn Per Unit", IM_COL32(245, 205, 95, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        if (auto* data = get_if<SourceHistorySpriteTrailPathFollowModuleData>(&module.data))
        {
            Add_FloatTarget(targets, emitter, module, data->speed, "speed", "경로 따라가기 속도", IM_COL32(120, 220, 255, 255), requireActiveCurve);
            Add_FloatTarget(targets, emitter, module, data->startDelay, "startDelay", "경로 따라가기 시작 딜레이", IM_COL32(125, 190, 255, 255), requireActiveCurve);
        }
        break;
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        if (auto* data = get_if<SourceHistorySpriteTrailPathReplayModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->drainCurve, "drainCurve", "경로 리플레이 흡수 곡선", IM_COL32(120, 235, 210, 255), requireActiveCurve);
        break;
    case AuthoringModuleType::RibbonOrientation:
        break;
    case AuthoringModuleType::MaterialScalarModulation:
        if (auto* data = get_if<MaterialScalarModulationModuleData>(&module.data))
        {
            for (size_t index = 0; index < data->modulators.size(); ++index)
            {
                MaterialScalarModulatorData& modulator = data->modulators[index];
                Add_FloatTarget(
                    targets,
                    emitter,
                    module,
                    modulator.distribution,
                    "modulator" + to_string(index) + ".distribution",
                    "#" + to_string(index + 1) + " " + Get_MaterialScalarTargetLabel(modulator.targetField),
                    IM_COL32(255, 145, 70, 255),
                    requireActiveCurve
                );
            }

            for (size_t index = 0; index < data->coreColorRgbModulators.size(); ++index)
            {
                MaterialCoreColorRgbModulatorData& modulator = data->coreColorRgbModulators[index];
                Add_Vector3Target(
                    targets,
                    emitter,
                    module,
                    modulator.distribution,
                    "coreColorRgbModulator" + to_string(index) + ".distribution",
                    "#" + to_string(data->modulators.size() + index + 1) + " Core Color RGB",
                    true,
                    requireActiveCurve
                );
            }

            for (size_t index = 0; index < data->vec2Modulators.size(); ++index)
            {
                MaterialVec2ModulatorData& modulator = data->vec2Modulators[index];
                Add_Vector2Target(
                    targets,
                    emitter,
                    module,
                    modulator.distribution,
                    "vec2Modulator" + to_string(index) + ".distribution",
                    "#" + to_string(data->modulators.size() + data->coreColorRgbModulators.size() + index + 1) + " " + Get_MaterialVec2TargetLabel(
                        modulator.targetField
                    ),
                    requireActiveCurve
                );
            }
        }
        break;
    default:
        break;
    }

    return targets;
}

vector<CurveEditor_View::CurveTarget> CurveEditor_View::Resolve_PinnedTargets()
{
    vector<CurveTarget> targets{};
    targets.reserve(_pinnedTracks.size());

    for (const PinnedTrack& track : _pinnedTracks)
    {
        if (optional<CurveTarget> target = Resolve_Target(track.identity))
            targets.push_back(*target);
    }

    return targets;
}

optional<CurveEditor_View::CurveTarget> CurveEditor_View::Resolve_Target(const CurveTargetIdentity& identity) const
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView)
        return nullopt;

    AuthoringEmitter* emitter = emitterView->Find_Emitter(identity.emitterId);
    AuthoringModule* module = emitterView->Find_Module(identity.emitterId, identity.moduleId);
    if (nullptr == emitter || nullptr == module || module->type != identity.moduleType)
        return nullopt;

    vector<CurveTarget> targets = Collect_ModuleTargets(*emitter, *module);
    for (CurveTarget& target : targets)
    {
        if (Is_SameIdentity(target.identity, identity))
            return target;
    }

    return nullopt;
}

optional<size_t> CurveEditor_View::Find_PinnedTrackIndex(const CurveTargetIdentity& identity) const
{
    for (size_t index = 0; index < _pinnedTracks.size(); ++index)
    {
        if (Is_SameIdentity(_pinnedTracks[index].identity, identity))
            return index;
    }

    return nullopt;
}

optional<size_t> CurveEditor_View::Find_ResolvedTargetIndex(const vector<CurveTarget>& targets, const CurveTargetIdentity& identity) const
{
    for (size_t index = 0; index < targets.size(); ++index)
    {
        if (Is_SameIdentity(targets[index].identity, identity))
            return index;
    }

    return nullopt;
}

optional<CurveEditor_View::CurveTarget> CurveEditor_View::Get_ActiveTarget(const vector<CurveTarget>& targets) const
{
    if (_activeTrack.has_value())
    {
        if (const optional<size_t> index = Find_ResolvedTargetIndex(targets, *_activeTrack))
            return targets[*index];
    }

    if (!targets.empty())
        return targets.front();

    return nullopt;
}

void CurveEditor_View::Add_FloatTarget(
    vector<CurveTarget>& targets,
    AuthoringEmitter& emitter,
    AuthoringModule& module,
    FloatDistributionData& distribution,
    const string& propertyId,
    const string& label,
    ImU32 color,
    bool requireActiveCurve)
{
    if (requireActiveCurve && nullptr == Resolve_CurvePayload(distribution))
        return;

    CurveTargetIdentity identity{};
    identity.emitterId = emitter.id;
    identity.moduleId = module.id;
    identity.moduleType = module.type;
    identity.propertyId = propertyId;
    identity.channel = CurveTargetChannel::Value;

    targets.push_back(
        CurveTarget{
            identity,
            &emitter,
            &module,
            &distribution,
            Build_Label(emitter, module, label, CurveTargetChannel::Value),
            label,
            CurveTargetPayloadKind::Float,
            CurveTargetChannel::Value,
            color
        }
    );
}

void CurveEditor_View::Add_Vector2Target(
    vector<CurveTarget>& targets,
    AuthoringEmitter& emitter,
    AuthoringModule& module,
    Vector2DistributionData& distribution,
    const string& propertyId,
    const string& label,
    bool requireActiveCurve)
{
    if (requireActiveCurve && nullptr == Resolve_CurvePayload(distribution))
        return;

    for (const CurveTargetChannel channel : { CurveTargetChannel::X, CurveTargetChannel::Y })
    {
        CurveTargetIdentity identity{};
        identity.emitterId = emitter.id;
        identity.moduleId = module.id;
        identity.moduleType = module.type;
        identity.propertyId = propertyId;
        identity.channel = channel;

        targets.push_back(
            CurveTarget{
                identity,
                &emitter,
                &module,
                &distribution,
                Build_Label(emitter, module, label, channel),
                label,
                CurveTargetPayloadKind::Vector2,
                channel,
                0
            }
        );
    }
}

void CurveEditor_View::Add_Vector3Target(
    vector<CurveTarget>& targets,
    AuthoringEmitter& emitter,
    AuthoringModule& module,
    Vector3DistributionData& distribution,
    const string& propertyId,
    const string& label,
    bool allowCurveTarget,
    bool requireActiveCurve)
{
    if (!allowCurveTarget || (requireActiveCurve && nullptr == Resolve_CurvePayload(distribution)))
        return;

    for (const CurveTargetChannel channel : { CurveTargetChannel::X, CurveTargetChannel::Y, CurveTargetChannel::Z })
    {
        CurveTargetIdentity identity{};
        identity.emitterId = emitter.id;
        identity.moduleId = module.id;
        identity.moduleType = module.type;
        identity.propertyId = propertyId;
        identity.channel = channel;

        targets.push_back(
            CurveTarget{
                identity,
                &emitter,
                &module,
                &distribution,
                Build_Label(emitter, module, label, channel),
                label,
                CurveTargetPayloadKind::Vector3,
                channel,
                0
            }
        );
    }
}

void CurveEditor_View::Add_ColorTarget(
    vector<CurveTarget>& targets,
    AuthoringEmitter& emitter,
    AuthoringModule& module,
    ColorDistributionData& distribution,
    const string& propertyId,
    const string& label,
    bool requireActiveCurve)
{
    if (requireActiveCurve && nullptr == Resolve_CurvePayload(distribution))
        return;

    for (const CurveTargetChannel channel : { CurveTargetChannel::R, CurveTargetChannel::G, CurveTargetChannel::B })
    {
        CurveTargetIdentity identity{};
        identity.emitterId = emitter.id;
        identity.moduleId = module.id;
        identity.moduleType = module.type;
        identity.propertyId = propertyId;
        identity.channel = channel;

        targets.push_back(
            CurveTarget{
                identity,
                &emitter,
                &module,
                &distribution,
                Build_Label(emitter, module, label, channel),
                label,
                CurveTargetPayloadKind::Color,
                channel,
                0
            }
        );
    }
}

void CurveEditor_View::Pin_Targets(const vector<CurveTarget>& targets)
{
    optional<CurveTargetIdentity> firstPinned{};

    for (const CurveTarget& target : targets)
    {
        if (!Find_PinnedTrackIndex(target.identity).has_value())
            _pinnedTracks.push_back(PinnedTrack{ target.identity, true });

        if (!firstPinned.has_value())
            firstPinned = target.identity;
    }

    if (firstPinned.has_value())
    {
        _activeTrack = firstPinned;
        _selectedKeyIndex.reset();
        Set_Open(true);
    }
}

void CurveEditor_View::Remove_StalePinnedTracks(const vector<CurveTarget>& targets)
{
    erase_if(
        _pinnedTracks,
        [this, &targets](const PinnedTrack& track)
        {
            return !Find_ResolvedTargetIndex(targets, track.identity).has_value();
        }
    );

    if (_activeTrack.has_value() && !Find_ResolvedTargetIndex(targets, *_activeTrack).has_value())
    {
        _activeTrack.reset();
        _selectedKeyIndex.reset();
    }
}

void CurveEditor_View::Ensure_ActiveTrack(const vector<CurveTarget>& targets)
{
    if (targets.empty())
    {
        _activeTrack.reset();
        _selectedKeyIndex.reset();
        return;
    }

    if (!_activeTrack.has_value() || !Find_ResolvedTargetIndex(targets, *_activeTrack).has_value())
    {
        _activeTrack = targets.front().identity;
        _selectedKeyIndex.reset();
    }
}

void CurveEditor_View::Begin_CurveAuthoringEdit(const CurveTarget& target, const string& description)
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView)
        return;

    if (_hasPendingCurveEdit)
    {
        if (Is_SameIdentity(_pendingCurveTarget, target.identity))
            return;

        Commit_CurveAuthoringEdit();
    }

    _hasPendingCurveEdit = true;
    _pendingCurveBeforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    _pendingCurveTarget = target.identity;
    _pendingCurveDescription = description;
}

void CurveEditor_View::Commit_CurveAuthoringEdit()
{
    if (!_hasPendingCurveEdit)
        return;

    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (emitterView)
    {
        Emitter_View::AuthoringSnapshot afterSnapshot = emitterView->Capture_AuthoringSnapshot();
        afterSnapshot.selectedEmitterIndex = _pendingCurveBeforeSnapshot.selectedEmitterIndex;
        afterSnapshot.selectedTypeData = _pendingCurveBeforeSnapshot.selectedTypeData;
        afterSnapshot.selectedModuleIndex = _pendingCurveBeforeSnapshot.selectedModuleIndex;
        emitterView->Execute_AuthoringSnapshotCommand(
            _pendingCurveBeforeSnapshot,
            afterSnapshot,
            _pendingCurveDescription
        );
    }

    _hasPendingCurveEdit = false;
    _pendingCurveBeforeSnapshot = {};
    _pendingCurveTarget = {};
    _pendingCurveDescription.clear();
}

bool CurveEditor_View::Execute_CurveAuthoringEdit(const CurveTarget& target, const string& description, const function<bool()>& edit)
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView || !edit)
        return false;

    Commit_CurveAuthoringEdit();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    if (!edit())
        return false;

    if (nullptr != target.emitter)
        target.emitter->previewDirty = true;
    emitterView->MarkDirty();
    MarkDirty();

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView->Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = beforeSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = beforeSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = beforeSnapshot.selectedModuleIndex;
    emitterView->Execute_AuthoringSnapshotCommand(beforeSnapshot, afterSnapshot, description);
    return true;
}

void CurveEditor_View::Mark_CurveChanged(const CurveTarget& target)
{
    if (nullptr != target.emitter)
        target.emitter->previewDirty = true;

    if (const Shared<Emitter_View> emitterView = Resolve_EmitterView())
        emitterView->MarkDirty();

    MarkDirty();
}

bool CurveEditor_View::Convert_ActiveTargetToConstantCurve(const CurveTarget& target)
{
    if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        **floatDistribution = FloatDistributionData::Make_ConstantCurve(Get_DefaultValue(**floatDistribution));
        target.emitter->previewDirty = true;
        _selectedKeyIndex = 0;
        return true;
    }

    if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        **vector2Distribution = Vector2DistributionData::Make_ConstantCurve(Get_DefaultValue(**vector2Distribution));
        target.emitter->previewDirty = true;
        _selectedKeyIndex = 0;
        return true;
    }

    if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
    {
        **vector3Distribution = Vector3DistributionData::Make_ConstantCurve(Get_DefaultValue(**vector3Distribution));
        target.emitter->previewDirty = true;
        _selectedKeyIndex = 0;
        return true;
    }

    if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
    {
        **colorDistribution = ColorDistributionData::Make_ConstantCurve(Get_DefaultValue(**colorDistribution));
        target.emitter->previewDirty = true;
        _selectedKeyIndex = 0;
        return true;
    }

    return false;
}

CurveEditor_View::GraphValueRange CurveEditor_View::Compute_GraphValueRange(const vector<CurveTarget>& targets)
{
    float minValue = 0.f;
    float maxValue = 1.f;
    bool hasVisibleCurve = false;

    for (const CurveTarget& target : targets)
    {
        optional<size_t> pinnedIndex = Find_PinnedTrackIndex(target.identity);
        if (!pinnedIndex.has_value() || !_pinnedTracks[*pinnedIndex].visible)
            continue;

        if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
            if (nullptr == curve)
                continue;

            Normalize_CurveKeys(*curve);
            for (const FloatCurveKeyData& key : curve->keys)
            {
                const float value = Get_TargetValue(target, key);
                minValue = min(minValue, value);
                maxValue = max(maxValue, value);
                hasVisibleCurve = true;
            }
        }
        else if (auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution);
            if (nullptr == curve)
                continue;

            Normalize_CurveKeys(*curve);
            for (const Vector2CurveKeyData& key : curve->keys)
            {
                const float value = Get_TargetValue(target, key);
                minValue = min(minValue, value);
                maxValue = max(maxValue, value);
                hasVisibleCurve = true;
            }
        }
        else if (auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
        {
            ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution);
            if (nullptr == curve)
                continue;

            Normalize_CurveKeys(*curve);
            for (const Vector3CurveKeyData& key : curve->keys)
            {
                const float value = Get_TargetValue(target, key);
                minValue = min(minValue, value);
                maxValue = max(maxValue, value);
                hasVisibleCurve = true;
            }
        }
        else if (auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
        {
            ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution);
            if (nullptr == curve)
                continue;

            Normalize_CurveKeys(*curve);
            for (const ColorCurveKeyData& key : curve->keys)
            {
                const float value = Get_TargetValue(target, key);
                minValue = min(minValue, value);
                maxValue = max(maxValue, value);
                hasVisibleCurve = true;
            }
        }
    }

    if (!hasVisibleCurve)
    {
        minValue = 0.f;
        maxValue = 1.f;
    }

    const float margin = max((maxValue - minValue) * 0.1f, 0.1f);
    return GraphValueRange{ minValue - margin, maxValue + margin };
}

bool CurveEditor_View::Is_GraphDragActiveFor(const CurveTarget& target, size_t keyIndex) const
{
    return _graphDrag.active &&
           _graphDrag.keyIndex == keyIndex &&
           Is_SameIdentity(_graphDrag.target, target.identity);
}

void CurveEditor_View::Begin_GraphKeyDrag(
    const CurveTarget& target,
    size_t keyIndex,
    const ImVec2& mousePos,
    float startTime,
    float startValue,
    const GraphValueRange& valueRange)
{
    if (Is_GraphDragActiveFor(target, keyIndex))
        return;

    _graphDrag.active = true;
    _graphDrag.target = target.identity;
    _graphDrag.keyIndex = keyIndex;
    _graphDrag.startMousePos = mousePos;
    _graphDrag.startTime = startTime;
    _graphDrag.startValue = startValue;
    _graphDrag.startRange = valueRange;
}

void CurveEditor_View::End_GraphKeyDrag()
{
    _graphDrag = GraphDragState{};
}

void CurveEditor_View::Draw_EmptyState()
{
    ImGui::TextDisabled("Emitter 창의 모듈 커브 버튼으로 편집할 커브를 고정하세요.");
}

void CurveEditor_View::Draw_TrackList(const vector<CurveTarget>& targets)
{
    ImGui::BeginChild("##CurveTrackList", ImVec2(_trackListWidth, 0.f), true);
    ImGui::TextDisabled("Tracks");
    ImGui::Separator();

    const auto build_base_label = [](const CurveTarget& target)
    {
        if (target.channel == CurveTargetChannel::Value)
            return target.propertyLabel;

        return string{ Get_ChannelLabel(target.channel) };
    };

    vector<string> baseLabels{};
    baseLabels.reserve(targets.size());
    for (const CurveTarget& target : targets)
    {
        if (Find_PinnedTrackIndex(target.identity).has_value())
            baseLabels.push_back(build_base_label(target));
    }

    for (const CurveTarget& target : targets)
    {
        optional<size_t> pinnedIndex = Find_PinnedTrackIndex(target.identity);
        if (!pinnedIndex.has_value())
            continue;

        const bool selected = _activeTrack.has_value() && Is_SameIdentity(*_activeTrack, target.identity);
        string trackLabel = build_base_label(target);
        if (target.channel != CurveTargetChannel::Value &&
            ranges::count(baseLabels, trackLabel) > 1)
            trackLabel = target.propertyLabel + " " + trackLabel;

        ImGui::PushID(static_cast<int32>(*pinnedIndex));
        if (ImGui::Selectable("##Track", selected, 0, ImVec2{ 0.f, ImGui::GetFrameHeight() }))
        {
            _activeTrack = target.identity;
            _selectedKeyIndex.reset();
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const float swatchSize = 9.f;
        const ImVec2 swatchMin{ rowMin.x + 7.f, rowMin.y + (rowMax.y - rowMin.y - swatchSize) * 0.5f };
        drawList->AddRectFilled(
            swatchMin,
            ImVec2{ swatchMin.x + swatchSize, swatchMin.y + swatchSize },
            Get_TargetColor(target),
            2.f
        );
        drawList->AddText(
            ImVec2{ rowMin.x + 23.f, rowMin.y + (rowMax.y - rowMin.y - ImGui::GetTextLineHeight()) * 0.5f },
            ImGui::GetColorU32(ImGuiCol_Text),
            trackLabel.c_str()
        );
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void CurveEditor_View::Draw_TargetHeader(CurveTarget& target)
{
    string headerLabel = nullptr != target.module && !target.module->displayName.empty()
                         ? String::ToString(target.module->displayName)
                         : target.propertyLabel;
    if (target.channel != CurveTargetChannel::Value)
    {
        headerLabel += " ";
        headerLabel += Get_ChannelLabel(target.channel);
    }

    ImGui::TextUnformatted(headerLabel.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("active");

    bool hasCurve = false;
    size_t keyCount = 0;
    if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
        {
            Normalize_CurveKeys(*curve);
            keyCount = curve->keys.size();
            hasCurve = true;
        }
    }
    else if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
        {
            Normalize_CurveKeys(*curve);
            keyCount = curve->keys.size();
            hasCurve = true;
        }
    }
    else if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
    {
        if (ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
        {
            Normalize_CurveKeys(*curve);
            keyCount = curve->keys.size();
            hasCurve = true;
        }
    }
    else if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
    {
        if (ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
        {
            Normalize_CurveKeys(*curve);
            keyCount = curve->keys.size();
            hasCurve = true;
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Keys: %zu / %zu", keyCount, kDistributionCurveMaxKeys);

    if (!hasCurve)
    {
        if (ImGui::Button("Convert to Constant Curve"))
        {
            Execute_CurveAuthoringEdit(
                target,
                "Convert Curve Target",
                [this, &target]
                {
                    return Convert_ActiveTargetToConstantCurve(target);
                }
            );
        }
        return;
    }

    const auto get_bulk_interpolation_label = [&target]() -> const char*
    {
        if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            if (const ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
                return Get_CurveBulkInterpolationLabel(curve->keys);
        }
        else if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            if (const ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
                return Get_CurveBulkInterpolationLabel(curve->keys);
        }
        else if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
        {
            if (const ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
                return Get_CurveBulkInterpolationLabel(curve->keys);
        }
        else if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
        {
            if (const ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
                return Get_CurveBulkInterpolationLabel(curve->keys);
        }

        return "Linear";
    };

    const auto apply_bulk_interpolation = [this, &target](FloatCurveInterpolationMode mode)
    {
        Execute_CurveAuthoringEdit(
            target,
            "Apply Curve Interpolation To All Keys",
            [this, &target, mode]
            {
                if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
                {
                    if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
                    {
                        if (!Apply_CurveBulkInterpolationMode(curve->keys, mode))
                            return false;

                        Normalize_CurveKeys(*curve);
                        return true;
                    }
                }
                else if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
                {
                    if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
                    {
                        if (!Apply_CurveBulkInterpolationMode(curve->keys, mode))
                            return false;

                        Normalize_CurveKeys(*curve);
                        return true;
                    }
                }
                else if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
                {
                    if (ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
                    {
                        if (!Apply_CurveBulkInterpolationMode(curve->keys, mode))
                            return false;

                        Normalize_CurveKeys(*curve);
                        return true;
                    }
                }
                else if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
                {
                    if (ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
                    {
                        if (!Apply_CurveBulkInterpolationMode(curve->keys, mode))
                            return false;

                        Normalize_CurveKeys(*curve);
                        return true;
                    }
                }

                return false;
            }
        );
    };

    const bool canAddKey = keyCount < kDistributionCurveMaxKeys;
    if (!canAddKey)
        ImGui::BeginDisabled();
    if (ImGui::Button("Add Key"))
    {
        Execute_CurveAuthoringEdit(
            target,
            "Add Curve Key",
            [this, &target]
            {
                const auto select_added_key = [this](const auto& keys, float time)
                {
                    optional<size_t> addedKeyIndex{};
                    for (size_t index = 0; index < keys.size(); ++index)
                    {
                        if (keys[index].time == time)
                            addedKeyIndex = index;
                    }
                    _selectedKeyIndex = addedKeyIndex;
                };

                if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
                {
                    ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
                    if (nullptr == curve)
                        return false;

                    const float time = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size()
                                       ? clamp(curve->keys[*_selectedKeyIndex].time + 0.1f, 0.f, 1.f)
                                       : 0.5f;
                    const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                    curve->keys.push_back(FloatCurveKeyData{ time, Evaluate_Curve(*curve, time), 0.f, 0.f, interpolationMode });
                    Normalize_CurveKeys(*curve);
                    select_added_key(curve->keys, time);
                    return true;
                }
                if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
                {
                    ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution);
                    if (nullptr == curve)
                        return false;

                    const float time = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size()
                                       ? clamp(curve->keys[*_selectedKeyIndex].time + 0.1f, 0.f, 1.f)
                                       : 0.5f;
                    const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                    curve->keys.push_back(Vector2CurveKeyData{ time, Evaluate_Curve(*curve, time), Vec2{}, Vec2{}, interpolationMode });
                    Normalize_CurveKeys(*curve);
                    select_added_key(curve->keys, time);
                    return true;
                }
                if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
                {
                    ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution);
                    if (nullptr == curve)
                        return false;

                    const float time = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size()
                                       ? clamp(curve->keys[*_selectedKeyIndex].time + 0.1f, 0.f, 1.f)
                                       : 0.5f;
                    const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                    curve->keys.push_back(Vector3CurveKeyData{ time, Evaluate_Curve(*curve, time), Vec3{}, Vec3{}, interpolationMode });
                    Normalize_CurveKeys(*curve);
                    select_added_key(curve->keys, time);
                    return true;
                }
                if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
                {
                    ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution);
                    if (nullptr == curve)
                        return false;

                    const float time = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size()
                                       ? clamp(curve->keys[*_selectedKeyIndex].time + 0.1f, 0.f, 1.f)
                                       : 0.5f;
                    const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                    curve->keys.push_back(ColorCurveKeyData{ time, Evaluate_Curve(*curve, time), Color{}, Color{}, interpolationMode });
                    Normalize_CurveKeys(*curve);
                    select_added_key(curve->keys, time);
                    return true;
                }

                return false;
            }
        );
    }
    if (!canAddKey)
        ImGui::EndDisabled();

    ImGui::SameLine();
    bool canDelete = false;
    if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        if (const ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
            canDelete = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size() && curve->keys.size() > 2;
    }
    else if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        if (const ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
            canDelete = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size() && curve->keys.size() > 2;
    }
    else if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
    {
        if (const ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
            canDelete = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size() && curve->keys.size() > 2;
    }
    else if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
    {
        if (const ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
            canDelete = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size() && curve->keys.size() > 2;
    }

    if (!canDelete)
        ImGui::BeginDisabled();
    if (ImGui::Button("Delete Key") && canDelete)
    {
        Execute_CurveAuthoringEdit(
            target,
            "Delete Curve Key",
            [this, &target]
            {
                if (!_selectedKeyIndex.has_value())
                    return false;

                if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
                {
                    if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
                    {
                        curve->keys.erase(curve->keys.begin() + static_cast<ptrdiff_t>(*_selectedKeyIndex));
                        _selectedKeyIndex.reset();
                        return true;
                    }
                }
                else if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
                {
                    if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
                    {
                        curve->keys.erase(curve->keys.begin() + static_cast<ptrdiff_t>(*_selectedKeyIndex));
                        _selectedKeyIndex.reset();
                        return true;
                    }
                }
                else if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
                {
                    if (ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
                    {
                        curve->keys.erase(curve->keys.begin() + static_cast<ptrdiff_t>(*_selectedKeyIndex));
                        _selectedKeyIndex.reset();
                        return true;
                    }
                }
                else if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
                {
                    if (ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
                    {
                        curve->keys.erase(curve->keys.begin() + static_cast<ptrdiff_t>(*_selectedKeyIndex));
                        _selectedKeyIndex.reset();
                        return true;
                    }
                }

                return false;
            }
        );
    }
    if (!canDelete)
        ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextUnformatted("All");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.f);
    if (ImGui::BeginCombo("##CurveEditorBulkInterpolation", get_bulk_interpolation_label()))
    {
        const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            if (ImGui::Selectable(Get_InterpolationLabel(mode), false) && enabled)
                apply_bulk_interpolation(mode);

            if (!enabled)
                ImGui::EndDisabled();
        };

        draw_item(FloatCurveInterpolationMode::Linear, true);
        draw_item(FloatCurveInterpolationMode::Constant, true);
        draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
        draw_item(FloatCurveInterpolationMode::CurveAuto, false);
        draw_item(FloatCurveInterpolationMode::CurveUser, false);
        draw_item(FloatCurveInterpolationMode::CurveBreak, false);
        ImGui::EndCombo();
    }
}

void CurveEditor_View::Draw_CurveGraph(const vector<CurveTarget>& targets, CurveTarget& activeTarget)
{
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = kGraphHeight;
    canvasSize.x = max(canvasSize.x, 260.f);

    ImGui::InvisibleButton("##CurveGraph", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive = ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(canvasPos, ImVec2{ canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y }, IM_COL32(24, 26, 32, 255));
    drawList->AddRect(canvasPos, ImVec2{ canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y }, IM_COL32(75, 78, 88, 255));

    const ImVec2 graphOrigin{ canvasPos.x + kGraphPadding, canvasPos.y + 12.f };
    const ImVec2 graphSize{ canvasSize.x - kGraphPadding * 2.f, canvasSize.y - 36.f };

    const GraphValueRange computedRange = Compute_GraphValueRange(targets);
    const float minValue = computedRange.minValue;
    const float maxValue = computedRange.maxValue;

    for (int32 line = 0; line <= 4; ++line)
    {
        const float ratio = static_cast<float>(line) / 4.f;
        const float x = graphOrigin.x + graphSize.x * ratio;
        const float y = graphOrigin.y + graphSize.y * ratio;
        drawList->AddLine(ImVec2{ x, graphOrigin.y }, ImVec2{ x, graphOrigin.y + graphSize.y }, IM_COL32(48, 52, 60, 255));
        drawList->AddLine(ImVec2{ graphOrigin.x, y }, ImVec2{ graphOrigin.x + graphSize.x, y }, IM_COL32(48, 52, 60, 255));
    }

    for (const CurveTarget& target : targets)
    {
        optional<size_t> pinnedIndex = Find_PinnedTrackIndex(target.identity);
        if (!pinnedIndex.has_value() || !_pinnedTracks[*pinnedIndex].visible)
            continue;

        const bool isActiveTarget = Is_SameIdentity(target.identity, activeTarget.identity);
        const ImU32 lineColor = Get_TargetColor(target);
        const float lineThickness = isActiveTarget ? 2.25f : 1.25f;

        // TODO :
        // 네 타입 모두 거의 동일. 중복을 20~30줄 공유 로직 + 짧은 4개 분기로 줄일 수 있을듯. 자세한건 아래 주석 참고

        // (1) 커브/키 타입,
        // (2) 샘플 값을 스칼라로 뽑아내는 방법(Evaluate_Curve 결과를 float는 그대로 쓰고 vector2/3/color는 Read_XChannel을 거침),
        // (3) 키 포인트 값을 뽑아내는 방법(float는 key.value를 직접 쓰는데 나머지 셋은 Get_TargetValue(target, key)를 씀)
        // 세 가지만 타입별로 주입받는 얇은 템플릿 함수 하나로 뽑아내면 좋겠다.

        if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
            if (nullptr == curve)
                continue;

            ImVec2 previousPoint = To_GraphPoint(graphOrigin, graphSize, 0.f, Evaluate_Curve(*curve, 0.f), minValue, maxValue);
            for (int32 sample = 1; sample <= 64; ++sample)
            {
                const float time = static_cast<float>(sample) / 64.f;
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, time, Evaluate_Curve(*curve, time), minValue, maxValue);
                drawList->AddLine(previousPoint, point, lineColor, lineThickness);
                previousPoint = point;
            }

            for (size_t index = 0; index < curve->keys.size(); ++index)
            {
                const FloatCurveKeyData& key = curve->keys[index];
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, key.value, minValue, maxValue);
                const bool selected = isActiveTarget && _selectedKeyIndex.has_value() && *_selectedKeyIndex == index;
                drawList->AddCircleFilled(point, selected ? 5.f : 3.5f, selected ? IM_COL32(100, 190, 255, 255) : lineColor);
            }
        }
        else if (auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution);
            if (nullptr == curve)
                continue;

            ImVec2 previousPoint = To_GraphPoint(
                graphOrigin,
                graphSize,
                0.f,
                Read_Vector2Channel(Evaluate_Curve(*curve, 0.f), target.channel),
                minValue,
                maxValue
            );
            for (int32 sample = 1; sample <= 64; ++sample)
            {
                const float time = static_cast<float>(sample) / 64.f;
                const ImVec2 point = To_GraphPoint(
                    graphOrigin,
                    graphSize,
                    time,
                    Read_Vector2Channel(Evaluate_Curve(*curve, time), target.channel),
                    minValue,
                    maxValue
                );
                drawList->AddLine(previousPoint, point, lineColor, lineThickness);
                previousPoint = point;
            }

            for (size_t index = 0; index < curve->keys.size(); ++index)
            {
                const Vector2CurveKeyData& key = curve->keys[index];
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(target, key), minValue, maxValue);
                const bool selected = isActiveTarget && _selectedKeyIndex.has_value() && *_selectedKeyIndex == index;
                drawList->AddCircleFilled(point, selected ? 5.f : 3.5f, selected ? IM_COL32(100, 190, 255, 255) : lineColor);
            }
        }
        else if (auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
        {
            ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution);
            if (nullptr == curve)
                continue;

            ImVec2 previousPoint = To_GraphPoint(
                graphOrigin,
                graphSize,
                0.f,
                Read_Vector3Channel(Evaluate_Curve(*curve, 0.f), target.channel),
                minValue,
                maxValue
            );
            for (int32 sample = 1; sample <= 64; ++sample)
            {
                const float time = static_cast<float>(sample) / 64.f;
                const ImVec2 point = To_GraphPoint(
                    graphOrigin,
                    graphSize,
                    time,
                    Read_Vector3Channel(Evaluate_Curve(*curve, time), target.channel),
                    minValue,
                    maxValue
                );
                drawList->AddLine(previousPoint, point, lineColor, lineThickness);
                previousPoint = point;
            }

            for (size_t index = 0; index < curve->keys.size(); ++index)
            {
                const Vector3CurveKeyData& key = curve->keys[index];
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(target, key), minValue, maxValue);
                const bool selected = isActiveTarget && _selectedKeyIndex.has_value() && *_selectedKeyIndex == index;
                drawList->AddCircleFilled(point, selected ? 5.f : 3.5f, selected ? IM_COL32(100, 190, 255, 255) : lineColor);
            }
        }
        else if (auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
        {
            ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution);
            if (nullptr == curve)
                continue;

            ImVec2 previousPoint = To_GraphPoint(
                graphOrigin,
                graphSize,
                0.f,
                Read_ColorChannel(Evaluate_Curve(*curve, 0.f), target.channel),
                minValue,
                maxValue
            );
            for (int32 sample = 1; sample <= 64; ++sample)
            {
                const float time = static_cast<float>(sample) / 64.f;
                const ImVec2 point = To_GraphPoint(
                    graphOrigin,
                    graphSize,
                    time,
                    Read_ColorChannel(Evaluate_Curve(*curve, time), target.channel),
                    minValue,
                    maxValue
                );
                drawList->AddLine(previousPoint, point, lineColor, lineThickness);
                previousPoint = point;
            }

            for (size_t index = 0; index < curve->keys.size(); ++index)
            {
                const ColorCurveKeyData& key = curve->keys[index];
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(target, key), minValue, maxValue);
                const bool selected = isActiveTarget && _selectedKeyIndex.has_value() && *_selectedKeyIndex == index;
                drawList->AddCircleFilled(point, selected ? 5.f : 3.5f, selected ? IM_COL32(100, 190, 255, 255) : lineColor);
            }
        }
    }

    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    if (_graphDrag.active &&
        (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !Is_SameIdentity(_graphDrag.target, activeTarget.identity)))
        End_GraphKeyDrag();

    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        optional<size_t> hitIndex{};
        float hitTime = 0.f;
        float hitValue = 0.f;
        if (auto* floatDistribution = get_if<FloatDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
            {
                for (size_t index = 0; index < curve->keys.size(); ++index)
                {
                    const FloatCurveKeyData& key = curve->keys[index];
                    const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, key.value, minValue, maxValue);
                    const float dx = point.x - mousePos.x;
                    const float dy = point.y - mousePos.y;
                    if (sqrtf(dx * dx + dy * dy) <= kKeyHitRadius)
                    {
                        hitIndex = index;
                        hitTime = key.time;
                        hitValue = key.value;
                        break;
                    }
                }
            }
        }
        else if (auto* vector2Distribution = get_if<Vector2DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
            {
                for (size_t index = 0; index < curve->keys.size(); ++index)
                {
                    const Vector2CurveKeyData& key = curve->keys[index];
                    const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(activeTarget, key), minValue, maxValue);
                    const float dx = point.x - mousePos.x;
                    const float dy = point.y - mousePos.y;
                    if (sqrtf(dx * dx + dy * dy) <= kKeyHitRadius)
                    {
                        hitIndex = index;
                        hitTime = key.time;
                        hitValue = Get_TargetValue(activeTarget, key);
                        break;
                    }
                }
            }
        }
        else if (auto* vector3Distribution = get_if<Vector3DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
            {
                for (size_t index = 0; index < curve->keys.size(); ++index)
                {
                    const Vector3CurveKeyData& key = curve->keys[index];
                    const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(activeTarget, key), minValue, maxValue);
                    const float dx = point.x - mousePos.x;
                    const float dy = point.y - mousePos.y;
                    if (sqrtf(dx * dx + dy * dy) <= kKeyHitRadius)
                    {
                        hitIndex = index;
                        hitTime = key.time;
                        hitValue = Get_TargetValue(activeTarget, key);
                        break;
                    }
                }
            }
        }
        else if (auto* colorDistribution = get_if<ColorDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
            {
                for (size_t index = 0; index < curve->keys.size(); ++index)
                {
                    const ColorCurveKeyData& key = curve->keys[index];
                    const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(activeTarget, key), minValue, maxValue);
                    const float dx = point.x - mousePos.x;
                    const float dy = point.y - mousePos.y;
                    if (sqrtf(dx * dx + dy * dy) <= kKeyHitRadius)
                    {
                        hitIndex = index;
                        hitTime = key.time;
                        hitValue = Get_TargetValue(activeTarget, key);
                        break;
                    }
                }
            }
        }
        _selectedKeyIndex = hitIndex;
        if (hitIndex.has_value())
            Begin_GraphKeyDrag(activeTarget, *hitIndex, mousePos, hitTime, hitValue, computedRange);
        else
            End_GraphKeyDrag();
    }

    if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        const float time = From_GraphTime(graphOrigin, graphSize, mousePos.x);
        const float value = Clamp_GraphEditedValue(
            activeTarget,
            From_GraphValue(graphOrigin, graphSize, mousePos.y, minValue, maxValue)
        );

        Execute_CurveAuthoringEdit(
            activeTarget,
            "Add Curve Key",
            [this, &activeTarget, time, value]
            {
                const auto select_added_key = [this](const auto& keys, float keyTime)
                {
                    optional<size_t> addedKeyIndex{};
                    for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex)
                    {
                        if (keys[keyIndex].time == keyTime)
                            addedKeyIndex = keyIndex;
                    }
                    _selectedKeyIndex = addedKeyIndex;
                };

                if (const auto* floatDistribution = get_if<FloatDistributionData*>(&activeTarget.distribution))
                {
                    if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
                    {
                        const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                        curve->keys.push_back(FloatCurveKeyData{ time, value, 0.f, 0.f, interpolationMode });
                        Normalize_CurveKeys(*curve);
                        select_added_key(curve->keys, time);
                        return true;
                    }
                }
                else if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&activeTarget.distribution))
                {
                    if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution))
                    {
                        const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                        Vector2CurveKeyData key{ time, Evaluate_Curve(*curve, time), Vec2{}, Vec2{}, interpolationMode };
                        Set_TargetValue(activeTarget, key, value);
                        curve->keys.push_back(key);
                        Normalize_CurveKeys(*curve);
                        select_added_key(curve->keys, time);
                        return true;
                    }
                }
                else if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&activeTarget.distribution))
                {
                    if (ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution))
                    {
                        const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                        Vector3CurveKeyData key{ time, Evaluate_Curve(*curve, time), Vec3{}, Vec3{}, interpolationMode };
                        Set_TargetValue(activeTarget, key, value);
                        curve->keys.push_back(key);
                        Normalize_CurveKeys(*curve);
                        select_added_key(curve->keys, time);
                        return true;
                    }
                }
                else if (const auto* colorDistribution = get_if<ColorDistributionData*>(&activeTarget.distribution))
                {
                    if (ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution))
                    {
                        const FloatCurveInterpolationMode interpolationMode = Get_SelectedKeyInterpolationMode(curve->keys, _selectedKeyIndex);
                        ColorCurveKeyData key{ time, Evaluate_Curve(*curve, time), Color{}, Color{}, interpolationMode };
                        Set_TargetValue(activeTarget, key, value);
                        curve->keys.push_back(key);
                        Normalize_CurveKeys(*curve);
                        select_added_key(curve->keys, time);
                        return true;
                    }
                }

                return false;
            }
        );
    }

    if (isActive && _selectedKeyIndex.has_value() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const size_t index = *_selectedKeyIndex;
        if (!_graphDrag.active || !Is_GraphDragActiveFor(activeTarget, index))
        {
            const float startTime = From_GraphTime(graphOrigin, graphSize, mousePos.x);
            const float startValue = From_GraphValue(graphOrigin, graphSize, mousePos.y, minValue, maxValue);
            Begin_GraphKeyDrag(activeTarget, index, mousePos, startTime, startValue, computedRange);
        }

        const ImVec2 mouseDelta{
            mousePos.x - _graphDrag.startMousePos.x,
            mousePos.y - _graphDrag.startMousePos.y
        };
        const float draggedTime = clamp(_graphDrag.startTime + mouseDelta.x / max(graphSize.x, 1.f), 0.f, 1.f);
        const float valueUnitsPerPixel = Get_DragValueUnitsPerPixel(
            _graphDrag.startRange.minValue,
            _graphDrag.startRange.maxValue,
            graphSize.y
        );
        const float draggedValue = Clamp_GraphEditedValue(
            activeTarget,
            _graphDrag.startValue - mouseDelta.y * valueUnitsPerPixel
        );

        if (auto* floatDistribution = get_if<FloatDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution); nullptr != curve && index < curve->keys.size())
            {
                Begin_CurveAuthoringEdit(activeTarget, "Edit Curve Key");
                FloatCurveKeyData& key = curve->keys[index];
                key.time = draggedTime;
                Set_TargetValue(activeTarget, key, draggedValue);
                Mark_CurveChanged(activeTarget);
            }
        }
        else if (auto* vector2Distribution = get_if<Vector2DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution); nullptr != curve && index < curve->keys.size())
            {
                Begin_CurveAuthoringEdit(activeTarget, "Edit Curve Key");
                Vector2CurveKeyData& key = curve->keys[index];
                key.time = draggedTime;
                Set_TargetValue(activeTarget, key, draggedValue);
                Mark_CurveChanged(activeTarget);
            }
        }
        else if (auto* vector3Distribution = get_if<Vector3DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution); nullptr != curve && index < curve->keys.size())
            {
                Begin_CurveAuthoringEdit(activeTarget, "Edit Curve Key");
                Vector3CurveKeyData& key = curve->keys[index];
                key.time = draggedTime;
                Set_TargetValue(activeTarget, key, draggedValue);
                Mark_CurveChanged(activeTarget);
            }
        }
        else if (auto* colorDistribution = get_if<ColorDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution); nullptr != curve && index < curve->keys.size())
            {
                Begin_CurveAuthoringEdit(activeTarget, "Edit Curve Key");
                ColorCurveKeyData& key = curve->keys[index];
                key.time = draggedTime;
                Set_TargetValue(activeTarget, key, draggedValue);
                Mark_CurveChanged(activeTarget);
            }
        }
    }
}

void CurveEditor_View::Draw_SelectedKeyInspector(const CurveTarget& target)
{
    if (!_selectedKeyIndex.has_value())
    {
        ImGui::TextDisabled("No key selected.");
        return;
    }

    bool changed = false;
    const TargetEditRange editRange = Compute_TargetEditRange(target);

    if (const auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
        if (nullptr == curve || *_selectedKeyIndex >= curve->keys.size())
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        FloatCurveKeyData& key = curve->keys[*_selectedKeyIndex];
        float timeValue = key.time;
        float keyValue = key.value;
        changed |= ImGui::DragFloat("Time", &timeValue, 0.005f, 0.f, 1.f, "%.3f");
        changed |= ImGui::DragFloat("Value", &keyValue, 0.01f, editRange.minValue, editRange.maxValue, "%.3f");

        if (ImGui::BeginCombo("Interpolation", Get_InterpolationLabel(key.interpolationMode)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = key.interpolationMode == mode;
                if (!enabled)
                    ImGui::BeginDisabled();
                if (ImGui::Selectable(Get_InterpolationLabel(mode), selected) && enabled)
                {
                    Execute_CurveAuthoringEdit(
                        target,
                        "Edit Curve Interpolation",
                        [&key, mode]
                        {
                            key.interpolationMode = mode;
                            return true;
                        }
                    );
                }
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_item(FloatCurveInterpolationMode::Linear, true);
            draw_item(FloatCurveInterpolationMode::Constant, true);
            draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
            draw_item(FloatCurveInterpolationMode::CurveAuto, false);
            draw_item(FloatCurveInterpolationMode::CurveUser, false);
            draw_item(FloatCurveInterpolationMode::CurveBreak, false);
            ImGui::EndCombo();
        }

        if (changed)
        {
            Begin_CurveAuthoringEdit(target, "Edit Curve Key");
            key.time = clamp(timeValue, 0.f, 1.f);
            key.value = Clamp_GraphEditedValue(target, keyValue);
            Normalize_CurveKeys(*curve);
            Mark_CurveChanged(target);
        }

        return;
    }

    if (const auto* vector2Distribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vector2Distribution);
        if (nullptr == curve || *_selectedKeyIndex >= curve->keys.size())
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        Vector2CurveKeyData& key = curve->keys[*_selectedKeyIndex];
        float timeValue = key.time;
        float activeValue = Get_TargetValue(target, key);
        changed |= ImGui::DragFloat("Time", &timeValue, 0.005f, 0.f, 1.f, "%.3f");
        if (ImGui::DragFloat(Get_ChannelLabel(target.channel), &activeValue, 0.01f, editRange.minValue, editRange.maxValue, "%.3f"))
            changed = true;

        const CurveTargetChannel pairedChannel = target.channel == CurveTargetChannel::Y ? CurveTargetChannel::X : CurveTargetChannel::Y;
        float pairedValue = Read_Vector2Channel(key.value, pairedChannel);
        ImGui::BeginDisabled();
        ImGui::DragFloat(Get_ChannelLabel(pairedChannel), &pairedValue, 0.01f, editRange.minValue, editRange.maxValue, "%.3f");
        ImGui::EndDisabled();

        if (ImGui::BeginCombo("Interpolation", Get_InterpolationLabel(key.interpolationMode)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = key.interpolationMode == mode;
                if (!enabled)
                    ImGui::BeginDisabled();
                if (ImGui::Selectable(Get_InterpolationLabel(mode), selected) && enabled)
                {
                    Execute_CurveAuthoringEdit(
                        target,
                        "Edit Curve Interpolation",
                        [&key, mode]
                        {
                            key.interpolationMode = mode;
                            return true;
                        }
                    );
                }
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_item(FloatCurveInterpolationMode::Linear, true);
            draw_item(FloatCurveInterpolationMode::Constant, true);
            draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
            draw_item(FloatCurveInterpolationMode::CurveAuto, false);
            draw_item(FloatCurveInterpolationMode::CurveUser, false);
            draw_item(FloatCurveInterpolationMode::CurveBreak, false);
            ImGui::EndCombo();
        }

        if (changed)
        {
            Begin_CurveAuthoringEdit(target, "Edit Curve Key");
            key.time = clamp(timeValue, 0.f, 1.f);
            Set_TargetValue(target, key, Clamp_GraphEditedValue(target, activeValue));
            Normalize_CurveKeys(*curve);
            Mark_CurveChanged(target);
        }

        return;
    }

    if (const auto* vector3Distribution = get_if<Vector3DistributionData*>(&target.distribution))
    {
        ConstantCurveVector3DistributionData* curve = Resolve_CurvePayload(**vector3Distribution);
        if (nullptr == curve || *_selectedKeyIndex >= curve->keys.size())
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        Vector3CurveKeyData& key = curve->keys[*_selectedKeyIndex];
        float timeValue = key.time;
        float activeValue = Get_TargetValue(target, key);
        changed |= ImGui::DragFloat("Time", &timeValue, 0.005f, 0.f, 1.f, "%.3f");
        if (ImGui::DragFloat(Get_ChannelLabel(target.channel), &activeValue, 0.01f, editRange.minValue, editRange.maxValue, "%.3f"))
            changed = true;

        for (const CurveTargetChannel channel : { CurveTargetChannel::X, CurveTargetChannel::Y, CurveTargetChannel::Z })
        {
            if (channel == target.channel)
                continue;

            float pairedValue = Read_Vector3Channel(key.value, channel);
            ImGui::BeginDisabled();
            ImGui::DragFloat(Get_ChannelLabel(channel), &pairedValue, 0.01f, editRange.minValue, editRange.maxValue, "%.3f");
            ImGui::EndDisabled();
        }

        if (ImGui::BeginCombo("Interpolation", Get_InterpolationLabel(key.interpolationMode)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = key.interpolationMode == mode;
                if (!enabled)
                    ImGui::BeginDisabled();
                if (ImGui::Selectable(Get_InterpolationLabel(mode), selected) && enabled)
                {
                    Execute_CurveAuthoringEdit(
                        target,
                        "Edit Curve Interpolation",
                        [&key, mode]
                        {
                            key.interpolationMode = mode;
                            return true;
                        }
                    );
                }
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_item(FloatCurveInterpolationMode::Linear, true);
            draw_item(FloatCurveInterpolationMode::Constant, true);
            draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
            draw_item(FloatCurveInterpolationMode::CurveAuto, false);
            draw_item(FloatCurveInterpolationMode::CurveUser, false);
            draw_item(FloatCurveInterpolationMode::CurveBreak, false);
            ImGui::EndCombo();
        }

        if (changed)
        {
            Begin_CurveAuthoringEdit(target, "Edit Curve Key");
            key.time = clamp(timeValue, 0.f, 1.f);
            Set_TargetValue(target, key, Clamp_GraphEditedValue(target, activeValue));
            Normalize_CurveKeys(*curve);
            Mark_CurveChanged(target);
        }

        return;
    }

    if (const auto* colorDistribution = get_if<ColorDistributionData*>(&target.distribution))
    {
        ConstantCurveColorDistributionData* curve = Resolve_CurvePayload(**colorDistribution);
        if (nullptr == curve || *_selectedKeyIndex >= curve->keys.size())
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        ColorCurveKeyData& key = curve->keys[*_selectedKeyIndex];
        float timeValue = key.time;
        float activeValue = Get_TargetValue(target, key);
        changed |= ImGui::DragFloat("Time", &timeValue, 0.005f, 0.f, 1.f, "%.3f");
        if (ImGui::DragFloat(Get_ChannelLabel(target.channel), &activeValue, 0.005f, editRange.minValue, editRange.maxValue, "%.3f"))
            changed = true;

        for (const CurveTargetChannel channel : { CurveTargetChannel::R, CurveTargetChannel::G, CurveTargetChannel::B })
        {
            if (channel == target.channel)
                continue;

            float pairedValue = Read_ColorChannel(key.value, channel);
            ImGui::BeginDisabled();
            ImGui::DragFloat(Get_ChannelLabel(channel), &pairedValue, 0.005f, editRange.minValue, editRange.maxValue, "%.3f");
            ImGui::EndDisabled();
        }

        if (ImGui::BeginCombo("Interpolation", Get_InterpolationLabel(key.interpolationMode)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = key.interpolationMode == mode;
                if (!enabled)
                    ImGui::BeginDisabled();
                if (ImGui::Selectable(Get_InterpolationLabel(mode), selected) && enabled)
                {
                    Execute_CurveAuthoringEdit(
                        target,
                        "Edit Curve Interpolation",
                        [&key, mode]
                        {
                            key.interpolationMode = mode;
                            return true;
                        }
                    );
                }
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_item(FloatCurveInterpolationMode::Linear, true);
            draw_item(FloatCurveInterpolationMode::Constant, true);
            draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
            draw_item(FloatCurveInterpolationMode::CurveAuto, false);
            draw_item(FloatCurveInterpolationMode::CurveUser, false);
            draw_item(FloatCurveInterpolationMode::CurveBreak, false);
            ImGui::EndCombo();
        }

        if (changed)
        {
            Begin_CurveAuthoringEdit(target, "Edit Curve Key");
            key.time = clamp(timeValue, 0.f, 1.f);
            Set_TargetValue(target, key, Clamp_GraphEditedValue(target, activeValue));
            Normalize_CurveKeys(*curve);
            Mark_CurveChanged(target);
        }
    }
}

Shared<CurveEditor_View> CurveEditor_View::Create()
{
    return make_shared<CurveEditor_View>();
}

NS_END
