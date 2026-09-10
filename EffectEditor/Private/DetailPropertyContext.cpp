#include "DetailPropertyContext.h"

#include <cfloat>
#include <climits>
#include "Helper_Distribution.h"
#include "Particle_Types.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr EmitterScreenAlignment kVisibleScreenAlignmentValues[] = {
        EmitterScreenAlignment::FacingCameraPosition,
        EmitterScreenAlignment::Rectangle,
        EmitterScreenAlignment::WorldUpFacingCamera,
        EmitterScreenAlignment::Square,
        EmitterScreenAlignment::AwayFromCenter,
        EmitterScreenAlignment::Velocity,
        EmitterScreenAlignment::WorldPlaneXY,
        EmitterScreenAlignment::WorldPlaneXZ,
    };

    constexpr const char* kVisibleScreenAlignmentLabels[] = {
        "Facing Camera Position",
        "Rectangle",
        "World Up Facing Camera",
        "Square",
        "Away from Center",
        "Velocity",
        "World Plane XY",
        "World Plane XZ",
    };

    static_assert(IM_ARRAYSIZE(kVisibleScreenAlignmentValues) == IM_ARRAYSIZE(kVisibleScreenAlignmentLabels));

    bool Is_SourceHistorySpriteTrailScreenAlignmentSupported(EmitterScreenAlignment value)
    {
        return value == EmitterScreenAlignment::FacingCameraPosition ||
               value == EmitterScreenAlignment::Rectangle ||
               value == EmitterScreenAlignment::WorldUpFacingCamera ||
               value == EmitterScreenAlignment::Square ||
               value == EmitterScreenAlignment::Velocity;
    }

    constexpr size_t kDistributionCurveMaxKeys{ kEffectDistributionCurveMaxKeys };

    float Apply_Range(float value, const AuthoringValueRange* range)
    {
        if (range == nullptr)
            return value;

        if (range->hasMin)
            value = max(value, range->minValue);
        if (range->hasMax)
            value = min(value, range->maxValue);

        return value;
    }

    Vec2 Apply_Range(Vec2 value, const AuthoringValueRange* range)
    {
        value.x = Apply_Range(value.x, range);
        value.y = Apply_Range(value.y, range);
        return value;
    }

    Vec3 Apply_Range(Vec3 value, const AuthoringValueRange* range)
    {
        value.x = Apply_Range(value.x, range);
        value.y = Apply_Range(value.y, range);
        value.z = Apply_Range(value.z, range);
        return value;
    }

    int Find_ScreenAlignmentVisibleIndex(EmitterScreenAlignment value)
    {
        for (int index = 0; index < IM_ARRAYSIZE(kVisibleScreenAlignmentValues); ++index)
        {
            if (kVisibleScreenAlignmentValues[index] == value)
                return index;
        }

        return -1;
    }

    const char* Get_ScreenAlignmentLabel(EmitterScreenAlignment value)
    {
        switch (value)
        {
        case EmitterScreenAlignment::Square:
            return "Square";
        case EmitterScreenAlignment::FacingCameraPosition:
            return "Facing Camera Position";
        case EmitterScreenAlignment::Rectangle:
            return "Rectangle";
        case EmitterScreenAlignment::WorldUpFacingCamera:
            return "World Up Facing Camera";
        case EmitterScreenAlignment::Velocity:
            return "Velocity";
        case EmitterScreenAlignment::AwayFromCenter:
            return "Away from Center";
        case EmitterScreenAlignment::WorldPlaneXY:
            return "World Plane XY";
        case EmitterScreenAlignment::WorldPlaneXZ:
            return "World Plane XZ";
        case EmitterScreenAlignment::TypeSpecific:
            return "Type Specific";
        case EmitterScreenAlignment::FacingCameraDistanceBlend:
            return "Facing Camera Distance Blend";
        default:
            return "Facing Camera Position";
        }
    }

    const char* Get_ScreenAlignmentTooltip(EmitterScreenAlignment value)
    {
        switch (value)
        {
        case EmitterScreenAlignment::FacingCameraPosition:
            return "파티클 위치에서 카메라 위치를 직접 바라보는 빌보드입니다.";

        case EmitterScreenAlignment::Rectangle:
            return "카메라 화면 평면에 맞춰 붙는 직사각형 빌보드입니다.";

        case EmitterScreenAlignment::WorldUpFacingCamera:
            return "카드 Y축을 World Y로 세우고, 카드 X축은 카메라 가시성을 유지하도록 잡는 직사각형 빌보드입니다.";

        case EmitterScreenAlignment::Square:
            return "카메라 화면 평면 기준이며 더 큰 축으로 정사각형을 만드는 빌보드입니다.";

        case EmitterScreenAlignment::AwayFromCenter:
            return "이미터 중심 기준 radial 방향을 사용합니다. 하위 정렬 방식에서 look 또는 texture 축 정렬을 고릅니다.";

        case EmitterScreenAlignment::Velocity:
            return "파티클의 현재 속도 방향을 사용합니다. 하위 정렬 방식에서 look 또는 texture 축 정렬을 고릅니다.";

        case EmitterScreenAlignment::WorldPlaneXY:
            return "월드 XY 평면에 고정합니다. Mesh는 local XY 평면을 카드 기준으로 봅니다.";

        case EmitterScreenAlignment::WorldPlaneXZ:
            return "월드 바닥 XZ 평면에 고정합니다. Mesh는 local XY 평면을 카드 기준으로 봅니다.";

        default:
            return "";
        }
    }

    const char* Get_SourceHistorySpriteTrailScreenAlignmentTooltip(EmitterScreenAlignment value)
    {
        switch (value)
        {
        case EmitterScreenAlignment::FacingCameraPosition:
            return "Stamp 위치는 path 위에 유지하고, 카드는 각 위치에서 카메라 위치를 바라봅니다.";

        case EmitterScreenAlignment::Rectangle:
            return "Legacy alias입니다. 카드 세로축은 World Up으로 유지하고, 카드 길이 축은 texture X/Y 중 어느 축을 길이로 볼지만 정합니다.";

        case EmitterScreenAlignment::WorldUpFacingCamera:
            return "카드 세로축은 World Up으로 유지하고, 카드 길이 축은 texture X/Y 중 어느 축을 길이로 볼지만 정합니다.";

        case EmitterScreenAlignment::Square:
            return "카메라 화면 평면 기준이며 더 큰 축으로 정사각형 stamp card를 만듭니다.";

        case EmitterScreenAlignment::Velocity:
            return "SourceHistorySpriteTrail에서는 실제 속도 대신 path tangent를 기준 방향으로 사용합니다.";

        default:
            return Get_ScreenAlignmentTooltip(value);
        }
    }

    EmitterScreenAlignment Resolve_ScreenAlignmentFallback(EmitterScreenAlignment value)
    {
        const int visibleIndex = Find_ScreenAlignmentVisibleIndex(value);
        if (visibleIndex >= 0)
            return kVisibleScreenAlignmentValues[visibleIndex];

        return EmitterScreenAlignment::FacingCameraPosition;
    }

    EmitterScreenAlignment Resolve_SourceHistorySpriteTrailScreenAlignmentFallback(EmitterScreenAlignment value)
    {
        return Is_SourceHistorySpriteTrailScreenAlignmentSupported(value)
               ? value
               : EmitterScreenAlignment::FacingCameraPosition;
    }
}

class DetailPropertyContext::BasicDetailPropertyDrawer
{
public: //## Behavior::BasicControls
    bool Begin_PropertyTable(const char* id)
    {
        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
        constexpr float resetColumnWidth{ 40.f };
        if (!ImGui::BeginTable(id, 3, tableFlags))
            return false;

        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, resetColumnWidth);
        return true;
    }

    void End_PropertyTable()
    {
        ImGui::EndTable();
    }

    void Draw_PropertyLabel(const char* label, const char* tooltip = nullptr)
    {
        ImGui::TableNextRow();
        if (!_rowTintStack.empty())
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, _rowTintStack.back());
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if (tooltip != nullptr && tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", tooltip);
        ImGui::TableSetColumnIndex(1);
    }

    bool Draw_TreePropertyLabel(const char* label, ImGuiTreeNodeFlags flags = 0)
    {
        ImGui::TableNextRow();
        if (!_rowTintStack.empty())
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, _rowTintStack.back());
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        const bool isOpen = ImGui::TreeNodeEx(label, flags | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::TableSetColumnIndex(1);
        return isOpen;
    }

    void Push_RowTint(ImU32 color)
    {
        _rowTintStack.push_back(color);
    }

    void Pop_RowTint()
    {
        if (!_rowTintStack.empty())
            _rowTintStack.pop_back();
    }

    void Push_LabelTint(ImU32 color)
    {
        _labelTintStack.push_back(color);
    }

    void Pop_LabelTint()
    {
        if (!_labelTintStack.empty())
            _labelTintStack.pop_back();
    }

    bool Draw_ResetButton(bool canReset)
    {
        constexpr float resetButtonWidth{ 28.f };

        ImGui::TableSetColumnIndex(2);
        if (!canReset)
            ImGui::BeginDisabled();

        const bool clicked = ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT "##Reset", ImVec2{ resetButtonWidth, 0.f });
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("기본값으로 리셋");

        if (!canReset)
            ImGui::EndDisabled();

        return canReset && clicked;
    }

    bool Draw_StringProperty(const char* label, string& value, const string& defaultValue, const char* tooltip = nullptr)
    {
        char buffer[256]{};
        const size_t copyLength = min(value.size(), sizeof(buffer) - 1);
        memcpy(buffer, value.c_str(), copyLength);

        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool inputChanged = ImGui::InputText("##Value", buffer, sizeof(buffer));
        bool changed = inputChanged;
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        if (inputChanged && !resetClicked)
            value = buffer;

        return changed;
    }

    bool Draw_BoolProperty(const char* label, bool& value, bool defaultValue, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        bool changed = ImGui::Checkbox("##Value", &value);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_FloatProperty(
        const char* label,
        float& value,
        float defaultValue,
        float speed = 0.01f,
        const char* tooltip = nullptr,
        const AuthoringValueRange* range = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::DragFloat("##Value", &value, speed);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        if (changed)
            value = Apply_Range(value, range);
        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::ColorEdit4(
            "##Value",
            &value.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar
        );
        if (changed)
            Clamp_Color01(value);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorRgbProperty(const char* label, Color& value, const Color& defaultValue, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        float rgb[3]{ value.R(), value.G(), value.B() };
        bool changed = ImGui::ColorEdit3(
            "##Value",
            rgb,
            ImGuiColorEditFlags_Float
        );
        if (changed)
        {
            value.R(clamp(rgb[0], 0.f, 1.f));
            value.G(clamp(rgb[1], 0.f, 1.f));
            value.B(clamp(rgb[2], 0.f, 1.f));
        }
        if (Draw_ResetButton(
            !Is_DefaultValue(value.R(), defaultValue.R()) ||
            !Is_DefaultValue(value.G(), defaultValue.G()) ||
            !Is_DefaultValue(value.B(), defaultValue.B())
        ))
        {
            value.R(defaultValue.R());
            value.G(defaultValue.G());
            value.B(defaultValue.B());
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int currentValue = static_cast<int>(value);
        bool changed = ImGui::DragInt("##Value", &currentValue, 1.f, 0, INT_MAX);
        if (changed)
            value = static_cast<uint32>(max(0, currentValue));
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_Vec2Property(
        const char* label,
        Vec2& value,
        const Vec2& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::DragFloat2("##Value", &value.x, speed);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        if (changed)
            value = Apply_Range(value, range);
        ImGui::PopID();
        return changed;
    }

    bool Draw_Vec3Property(
        const char* label,
        Vec3& value,
        const Vec3& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::DragFloat3("##Value", &value.x, speed);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        if (changed)
            value = Apply_Range(value, range);
        ImGui::PopID();
        return changed;
    }

    bool Draw_BurstListProperty(
        vector<SpawnModuleData::ParticleBurstData>& burstList,
        const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
    {
        bool changed = false;

        ImGui::TextUnformatted("버스트 목록");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PLUS "##AddBurst"))
        {
            burstList.push_back(SpawnModuleData::ParticleBurstData{});
            changed = true;
        }
        ImGui::SameLine();
        if (!Is_DefaultValue(burstList, defaultValue))
        {
            if (ImGui::SmallButton(ICON_FA_ARROW_ROTATE_LEFT "##ResetBurstList"))
            {
                burstList = defaultValue;
                changed = true;
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::SmallButton(ICON_FA_ARROW_ROTATE_LEFT "##ResetBurstList");
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("기본값으로 리셋");

        if (burstList.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("비어 있음");
            return changed;
        }

        for (size_t index = 0; index < burstList.size();)
        {
            ImGui::PushID(static_cast<int>(index));

            string label = "버스트 " + to_string(index);
            const bool isOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::SameLine();
            const bool remove = ImGui::Button(ICON_FA_TRASH "##RemoveBurst");

            bool entryChanged = false;
            if (isOpen)
            {
                if (!remove)
                {
                    if (Begin_PropertyTable("BurstEntry"))
                    {
                        const SpawnModuleData::ParticleBurstData defaultBurst =
                            index < defaultValue.size() ? defaultValue[index] : SpawnModuleData::ParticleBurstData{};
                        entryChanged |= Draw_BurstEntryProperty(burstList[index], defaultBurst);
                        End_PropertyTable();
                    }
                }
                ImGui::TreePop();
            }

            ImGui::PopID();

            if (remove)
            {
                burstList.erase(burstList.begin() + static_cast<vector<SpawnModuleData::ParticleBurstData>::difference_type>(index));
                changed = true;
                continue;
            }

            changed |= entryChanged;
            ++index;
        }

        return changed;
    }

    bool Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
    {
        const EmitterScreenAlignment fallbackValue = Resolve_ScreenAlignmentFallback(value);
        int currentIndex = Find_ScreenAlignmentVisibleIndex(fallbackValue);
        if (currentIndex < 0)
            currentIndex = 0;

        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;
        if (ImGui::BeginCombo("##Value", kVisibleScreenAlignmentLabels[currentIndex]))
        {
            for (int index = 0; index < IM_ARRAYSIZE(kVisibleScreenAlignmentValues); ++index)
            {
                const bool isSelected = currentIndex == index;
                if (ImGui::Selectable(kVisibleScreenAlignmentLabels[index], isSelected))
                {
                    currentIndex = index;
                    changed = true;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", Get_ScreenAlignmentTooltip(kVisibleScreenAlignmentValues[index]));

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        if (!changed)
            return false;

        if (currentIndex < 0 || currentIndex >= IM_ARRAYSIZE(kVisibleScreenAlignmentValues))
            value = EmitterScreenAlignment::FacingCameraPosition;
        else
            value = kVisibleScreenAlignmentValues[currentIndex];

        return true;
    }

    bool Draw_SourceHistorySpriteTrailScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
    {
        const EmitterScreenAlignment fallbackValue = Resolve_SourceHistorySpriteTrailScreenAlignmentFallback(value);
        int currentIndex = Find_ScreenAlignmentVisibleIndex(fallbackValue);
        if (currentIndex < 0)
            currentIndex = 0;

        bool valueChanged = false;
        if (value != fallbackValue)
        {
            value = fallbackValue;
            valueChanged = true;
        }

        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool comboChanged = false;
        if (ImGui::BeginCombo("##Value", kVisibleScreenAlignmentLabels[currentIndex]))
        {
            for (int index = 0; index < IM_ARRAYSIZE(kVisibleScreenAlignmentValues); ++index)
            {
                const EmitterScreenAlignment candidate = kVisibleScreenAlignmentValues[index];
                const bool isSelected = currentIndex == index;
                const bool supported = Is_SourceHistorySpriteTrailScreenAlignmentSupported(candidate);
                if (!supported)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(kVisibleScreenAlignmentLabels[index], isSelected) && supported)
                {
                    currentIndex = index;
                    comboChanged = true;
                }

                if (!supported)
                    ImGui::EndDisabled();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(
                        supported
                        ? "%s"
                        : "SourceHistorySpriteTrail에서는 이 화면 정렬을 사용하지 않습니다.",
                        Get_SourceHistorySpriteTrailScreenAlignmentTooltip(candidate)
                    );
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        ImGui::PopID();

        if (resetClicked)
        {
            value = Resolve_SourceHistorySpriteTrailScreenAlignmentFallback(defaultValue);
            return true;
        }

        if (!comboChanged)
            return valueChanged;

        if (currentIndex < 0 || currentIndex >= IM_ARRAYSIZE(kVisibleScreenAlignmentValues))
            value = EmitterScreenAlignment::FacingCameraPosition;
        else
            value = kVisibleScreenAlignmentValues[currentIndex];

        return true;
    }

    void Draw_ScreenAlignmentImplementationNote(EmitterScreenAlignment value) const
    {
        const int visibleIndex = Find_ScreenAlignmentVisibleIndex(value);
        if (visibleIndex >= 0)
            return;

        const char* currentLabel = Get_ScreenAlignmentLabel(value);
        const char* fallbackLabel = Get_ScreenAlignmentLabel(Resolve_ScreenAlignmentFallback(value));
        ImGui::TextDisabled("Current value '%s' is not implemented yet. Preview uses '%s'.", currentLabel, fallbackLabel);
    }

    bool Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
    {
        int currentIndex = 0;
        switch (value)
        {
        case EmitterSortMode::None: currentIndex = 0;
            break;
        case EmitterSortMode::ViewProjDepth: currentIndex = 1;
            break;
        case EmitterSortMode::DistanceToView: currentIndex = 2;
            break;
        case EmitterSortMode::AgeOldestFirst: currentIndex = 3;
            break;
        case EmitterSortMode::AgeNewestFirst: currentIndex = 4;
            break;
        default: currentIndex = 0;
            break;
        }

        static constexpr const char* kSortModeLabels[] = {
            "None",
            "View Proj Depth",
            "Distance to View",
            "Age Oldest First",
            "Age Newest First",
        };

        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool changed = ImGui::Combo("##Value", &currentIndex, kSortModeLabels, IM_ARRAYSIZE(kSortModeLabels));
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        if (!changed)
            return false;

        switch (currentIndex)
        {
        case 0: value = EmitterSortMode::None;
            break;
        case 1: value = EmitterSortMode::ViewProjDepth;
            break;
        case 2: value = EmitterSortMode::DistanceToView;
            break;
        case 3: value = EmitterSortMode::AgeOldestFirst;
            break;
        case 4: value = EmitterSortMode::AgeNewestFirst;
            break;
        default: value = EmitterSortMode::None;
            break;
        }

        return true;
    }

private: //## Data::Highlight
    vector<ImU32> _rowTintStack{};
    vector<ImU32> _labelTintStack{};

private: //## Helper::Reset
    static bool Is_DefaultValue(float value, float defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(uint32 value, uint32 defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(bool value, bool defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(EmitterScreenAlignment value, EmitterScreenAlignment defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(EmitterDirectionalAlignmentMode value, EmitterDirectionalAlignmentMode defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(EmitterSpriteTextureAxis value, EmitterSpriteTextureAxis defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(EmitterSortMode value, EmitterSortMode defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(const string& value, const string& defaultValue) { return value == defaultValue; }

    static bool Is_DefaultValue(const Vec2& value, const Vec2& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y;
    }

    static bool Is_DefaultValue(const Vec3& value, const Vec3& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z;
    }

    static bool Is_DefaultValue(const Color& value, const Color& defaultValue)
    {
        return value.x == defaultValue.x &&
               value.y == defaultValue.y &&
               value.z == defaultValue.z &&
               value.w == defaultValue.w;
    }

    static bool Is_DefaultValue(const SpawnModuleData::ParticleBurstData& value, const SpawnModuleData::ParticleBurstData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) && value.count == defaultValue.count;
    }

    static bool Is_DefaultValue(
        const vector<SpawnModuleData::ParticleBurstData>& value,
        const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
    {
        if (value.size() != defaultValue.size())
            return false;

        for (size_t index = 0; index < value.size(); ++index)
        {
            if (!Is_DefaultValue(value[index], defaultValue[index]))
                return false;
        }

        return true;
    }

    bool Draw_BurstEntryProperty(SpawnModuleData::ParticleBurstData& burst, const SpawnModuleData::ParticleBurstData& defaultValue)
    {
        bool changed = false;
        changed |= Draw_FloatProperty("시간", burst.time, defaultValue.time);
        changed |= Draw_UintProperty("개수", burst.count, defaultValue.count);
        return changed;
    }
};

class DetailPropertyContext::DistributionDetailPropertyDrawer
{
public: //## Behavior::DistributionControls
    explicit DistributionDetailPropertyDrawer(BasicDetailPropertyDrawer& basicDrawer)
        : _basicDrawer{ basicDrawer }
    {
    }

    bool Draw_FloatDistributionGroup(
        const char* label,
        FloatDistributionData& value,
        const FloatDistributionData& defaultValue,
        float speed = 0.01f,
        bool allowUniform = true,
        bool allowConstantCurve = true,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_FloatDistributionProperty("분포 타입", value, defaultValue, speed, allowUniform, allowConstantCurve, range, tooltip);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_Vector2DistributionGroup(
        const char* label,
        Vector2DistributionData& value,
        const Vector2DistributionData& defaultValue,
        float speed = 0.01f,
        bool allowConstantCurve = true,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_Vector2DistributionProperty("분포 타입", value, defaultValue, speed, allowConstantCurve, range, tooltip);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_Vector3DistributionGroup(
        const char* label,
        Vector3DistributionData& value,
        const Vector3DistributionData& defaultValue,
        float speed = 0.01f,
        bool allowConstantCurve = true,
        bool showConstantCurveAsUnimplemented = false,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_Vector3DistributionProperty(
                    "분포 타입",
                    value,
                    defaultValue,
                    speed,
                    allowConstantCurve,
                    showConstantCurveAsUnimplemented,
                    range,
                    tooltip
                );
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_ColorDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_ColorDistributionProperty("분포 타입", value, defaultValue);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_ColorRgbDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_ColorRgbDistributionProperty("분포 타입", value, defaultValue);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

private: //## Types::Distribution
    static const char* Get_DistributionRandomSeedModeLabel(DistributionRandomSeedMode mode)
    {
        switch (mode)
        {
        case DistributionRandomSeedMode::Default:
            return "기본값";
        case DistributionRandomSeedMode::Manual:
            return "수동";
        default:
            return "기본값";
        }
    }

    static uint32 Generate_RandomSeedValue()
    {
        static uint32 state{ 0xA341316Cu };
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state != 0u ? state : 0x6D2B79F5u;
    }

    bool Draw_RandomSeedPayload(DistributionRandomSeedData& data)
    {
        bool changed = false;

        ImGui::PushID("RandomSeed");
        _basicDrawer.Draw_PropertyLabel("랜덤 시드");
        ImGui::TextDisabled("Uniform 분포 샘플 seed");
        _basicDrawer.Draw_ResetButton(false);

        ImGui::PushID("RandomSeedMode");
        _basicDrawer.Draw_PropertyLabel("시드 모드", "같은 시드는 같은 랜덤 분포를 재현합니다.");
        DistributionRandomSeedMode nextMode = data.mode;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionRandomSeedModeLabel(nextMode)))
        {
            const DistributionRandomSeedMode candidates[] = {
                DistributionRandomSeedMode::Default,
                DistributionRandomSeedMode::Manual,
            };

            for (const DistributionRandomSeedMode candidate : candidates)
            {
                const bool selected = nextMode == candidate;
                if (ImGui::Selectable(Get_DistributionRandomSeedModeLabel(candidate), selected))
                    nextMode = candidate;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (data.mode != nextMode)
        {
            data.mode = nextMode;
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        ImGui::PushID("ManualSeed");
        _basicDrawer.Draw_PropertyLabel("시드");
        const float randomizeButtonWidth = ImGui::CalcTextSize("무작위").x + ImGui::GetStyle().FramePadding.x * 2.f;
        const float seedInputWidth = max(1.f, ImGui::GetContentRegionAvail().x - randomizeButtonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::SetNextItemWidth(seedInputWidth);
        const bool manualMode = data.mode == DistributionRandomSeedMode::Manual;
        if (!manualMode)
            ImGui::BeginDisabled();
        changed |= ImGui::InputScalar("##Value", ImGuiDataType_U32, &data.manualSeed);
        if (!manualMode)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("무작위"))
        {
            data.manualSeed = Generate_RandomSeedValue();
            data.mode = DistributionRandomSeedMode::Manual;
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        ImGui::PushID("UseInstanceSeed");
        _basicDrawer.Draw_PropertyLabel(
            "재생마다 변주",
            "켜면 이 이펙트를 재생할 때마다 생성되는 seed를 이 랜덤값 샘플에 섞습니다."
        );
        changed |= ImGui::Checkbox("##Value", &data.useInstanceSeed);
        if (data.useInstanceSeed)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("켜짐");
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        ImGui::PopID();
        return changed;
    }

    enum class DistributionValueKind : uint8
    {
        Float,
        Vec2,
        Vec3,
        Color,
    };

private: //## Data::Dependencies
    BasicDetailPropertyDrawer& _basicDrawer;

private: //## Helper::Color
    static ImVec4 Lerp_Color(const ImVec4& lhs, const ImVec4& rhs, float t)
    {
        return ImVec4{
            lhs.x + (rhs.x - lhs.x) * t,
            lhs.y + (rhs.y - lhs.y) * t,
            lhs.z + (rhs.z - lhs.z) * t,
            lhs.w + (rhs.w - lhs.w) * t,
        };
    }

    static ImU32 Get_DistributionPayloadRowTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 tint = Lerp_Color(frameBg, ImVec4{ 0.34f, 0.34f, 0.34f, frameBg.w }, 0.45f);
        return ImGui::GetColorU32(tint);
    }

    static ImU32 Get_DistributionKeyHeaderRowTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 tint = Lerp_Color(frameBg, ImVec4{ 0.38f, 0.38f, 0.38f, frameBg.w }, 0.55f);
        return ImGui::GetColorU32(tint);
    }

    static ImU32 Get_DistributionKeyBodyRowTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 tint = Lerp_Color(frameBg, ImVec4{ 0.35f, 0.35f, 0.35f, frameBg.w }, 0.40f);
        return ImGui::GetColorU32(tint);
    }

    static ImU32 Get_DistributionKeyLabelTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 headerActive = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
        const ImVec4 tint = Lerp_Color(frameBg, headerActive, 0.12f);
        return ImGui::GetColorU32(tint);
    }

private: //## Helper::Label
    static const char* Get_DistributionModeLabel(DistributionValueKind kind, DistributionMode mode)
    {
        switch (kind)
        {
        case DistributionValueKind::Float:
            switch (mode)
            {
            case DistributionMode::Constant: return "float 고정값";
            case DistributionMode::Uniform: return "float 랜덤값";
            case DistributionMode::ConstantCurve: return "float 고정값 커브 " ICON_FA_CHART_LINE;
            case DistributionMode::UniformCurve: return "float 랜덤 커브 (미구현)";
            case DistributionMode::ParticleParameter: return "float 파티클 파라미터 (미구현)";
            default: return "float 고정값";
            }

        case DistributionValueKind::Vec2:
            switch (mode)
            {
            case DistributionMode::Constant: return "Vec2 고정값";
            case DistributionMode::Uniform: return "Vec2 랜덤값";
            case DistributionMode::ConstantCurve: return "Vec2 고정값 커브 " ICON_FA_CHART_LINE;
            case DistributionMode::UniformCurve: return "Vec2 랜덤 커브 (미구현)";
            case DistributionMode::ParticleParameter: return "Vec2 파티클 파라미터 (미구현)";
            default: return "Vec2 고정값";
            }

        case DistributionValueKind::Vec3:
            switch (mode)
            {
            case DistributionMode::Constant: return "Vec3 고정값";
            case DistributionMode::Uniform: return "Vec3 랜덤값";
            case DistributionMode::ConstantCurve: return "Vec3 고정값 커브 " ICON_FA_CHART_LINE;
            case DistributionMode::UniformCurve: return "Vec3 랜덤 커브 (미구현)";
            case DistributionMode::ParticleParameter: return "Vec3 파티클 파라미터 (미구현)";
            default: return "Vec3 고정값";
            }

        case DistributionValueKind::Color:
            switch (mode)
            {
            case DistributionMode::Constant: return "Color 고정값";
            case DistributionMode::Uniform: return "Color 랜덤값";
            case DistributionMode::ConstantCurve: return "Color 고정값 커브 " ICON_FA_CHART_LINE;
            case DistributionMode::UniformCurve: return "Color 랜덤 커브 (미구현)";
            case DistributionMode::ParticleParameter: return "Color 파티클 파라미터 (미구현)";
            default: return "Color 고정값";
            }

        default:
            return "float 고정값";
        }
    }

    static const char* Get_FloatCurveInterpolationLabel(FloatCurveInterpolationMode mode)
    {
        switch (mode)
        {
        case FloatCurveInterpolationMode::Linear: return "Linear";
        case FloatCurveInterpolationMode::Constant: return "Constant";
        case FloatCurveInterpolationMode::CurveAuto: return "Curve Auto (미구현)";
        case FloatCurveInterpolationMode::CurveUser: return "Curve User (미구현)";
        case FloatCurveInterpolationMode::CurveBreak: return "Curve Break (미구현)";
        case FloatCurveInterpolationMode::CurveAutoClamped: return "Curve Auto Clamped";
        default: return "Linear";
        }
    }

    static void Draw_FloatCurveInterpolationSelectable(
        FloatCurveInterpolationMode candidate,
        bool enabled,
        FloatCurveInterpolationMode& current,
        bool& changed)
    {
        if (!enabled)
            ImGui::BeginDisabled();

        const bool isSelected = current == candidate;
        if (ImGui::Selectable(Get_FloatCurveInterpolationLabel(candidate), isSelected) && enabled)
        {
            current = candidate;
            changed = true;
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();

        if (!enabled)
            ImGui::EndDisabled();
    }

    template <typename TKey>
    static const char* Get_CurveBulkInterpolationLabel(const vector<TKey>& keys)
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
        return allSameMode ? Get_FloatCurveInterpolationLabel(firstMode) : "Mixed";
    }

    template <typename TKey>
    static bool Apply_CurveBulkInterpolationMode(vector<TKey>& keys, FloatCurveInterpolationMode mode)
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

    template <typename TKey>
    static bool Draw_CurveBulkInterpolationControl(vector<TKey>& keys)
    {
        bool changed = false;

        ImGui::SameLine();
        ImGui::TextUnformatted("전체 보간");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.f);
        if (ImGui::BeginCombo("##BulkInterpolation", Get_CurveBulkInterpolationLabel(keys)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = !keys.empty() && ranges::all_of(
                                          keys,
                                          [mode](const TKey& key)
                                          {
                                              return key.interpolationMode == mode;
                                          }
                                      );

                if (!enabled)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_FloatCurveInterpolationLabel(mode), selected) && enabled)
                    changed |= Apply_CurveBulkInterpolationMode(keys, mode);

                if (selected)
                    ImGui::SetItemDefaultFocus();

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

        return changed;
    }

    static float Evaluate_FloatCurveAutoClampedTangent(
        const FloatCurveKeyData* prevKey,
        const FloatCurveKeyData& key,
        const FloatCurveKeyData* nextKey)
    {
        if (prevKey == nullptr || nextKey == nullptr)
            return 0.f;

        const float dt = nextKey->time - prevKey->time;
        if (dt <= 0.f)
            return 0.f;

        return (nextKey->value - prevKey->value) / dt;
    }

    static void Normalize_FloatCurveKeys(ConstantCurveFloatDistributionData& value)
    {
        if (value.keys.empty())
        {
            value.keys = {
                FloatCurveKeyData{ 0.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
                FloatCurveKeyData{ 1.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
            };
            return;
        }

        ranges::sort(
            value.keys,
            [](const FloatCurveKeyData& lhs, const FloatCurveKeyData& rhs)
            {
                return lhs.time < rhs.time;
            }
        );

        for (FloatCurveKeyData& key : value.keys)
            key.time = clamp(key.time, 0.f, 1.f);

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            FloatCurveKeyData& key = value.keys[index];
            const FloatCurveKeyData* prevKey = index > 0 ? &value.keys[index - 1] : nullptr;
            const FloatCurveKeyData* nextKey = index + 1 < value.keys.size() ? &value.keys[index + 1] : nullptr;

            if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            {
                const float tangent = Evaluate_FloatCurveAutoClampedTangent(prevKey, key, nextKey);
                key.arriveTangent = tangent;
                key.leaveTangent = tangent;
            }
        }
    }

    static Vec2 Evaluate_Vector2CurveAutoClampedTangent(
        const Vector2CurveKeyData* prevKey,
        const Vector2CurveKeyData& key,
        const Vector2CurveKeyData* nextKey)
    {
        if (prevKey == nullptr || nextKey == nullptr)
            return Vec2{};

        const float dt = nextKey->time - prevKey->time;
        if (dt <= 0.f)
            return Vec2{};

        return Vec2{
            (nextKey->value.x - prevKey->value.x) / dt,
            (nextKey->value.y - prevKey->value.y) / dt
        };
    }

    static void Normalize_Vector2CurveKeys(ConstantCurveVector2DistributionData& value)
    {
        if (value.keys.empty())
        {
            value.keys = {
                Vector2CurveKeyData{ 0.f, Vec2{ 0.f, 0.f }, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear },
                Vector2CurveKeyData{ 1.f, Vec2{ 0.f, 0.f }, Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear }
            };
            return;
        }

        ranges::sort(
            value.keys,
            [](const Vector2CurveKeyData& lhs, const Vector2CurveKeyData& rhs)
            {
                return lhs.time < rhs.time;
            }
        );

        for (Vector2CurveKeyData& key : value.keys)
            key.time = clamp(key.time, 0.f, 1.f);

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            Vector2CurveKeyData& key = value.keys[index];
            const Vector2CurveKeyData* prevKey = index > 0 ? &value.keys[index - 1] : nullptr;
            const Vector2CurveKeyData* nextKey = index + 1 < value.keys.size() ? &value.keys[index + 1] : nullptr;

            if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            {
                const Vec2 tangent = Evaluate_Vector2CurveAutoClampedTangent(prevKey, key, nextKey);
                key.arriveTangent = tangent;
                key.leaveTangent = tangent;
            }
        }
    }

    static Vec3 Evaluate_Vector3CurveAutoClampedTangent(
        const Vector3CurveKeyData* prevKey,
        const Vector3CurveKeyData& key,
        const Vector3CurveKeyData* nextKey)
    {
        if (prevKey == nullptr || nextKey == nullptr)
            return Vec3{};

        const float dt = nextKey->time - prevKey->time;
        if (dt <= 0.f)
            return Vec3{};

        return Vec3{
            (nextKey->value.x - prevKey->value.x) / dt,
            (nextKey->value.y - prevKey->value.y) / dt,
            (nextKey->value.z - prevKey->value.z) / dt
        };
    }

    static void Normalize_Vector3CurveKeys(ConstantCurveVector3DistributionData& value)
    {
        if (value.keys.empty())
        {
            value.keys = {
                Vector3CurveKeyData{ 0.f, Vec3{ 0.f, 0.f, 0.f }, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear },
                Vector3CurveKeyData{ 1.f, Vec3{ 0.f, 0.f, 0.f }, Vec3{}, Vec3{}, FloatCurveInterpolationMode::Linear }
            };
            return;
        }

        ranges::sort(value.keys, [](const Vector3CurveKeyData& lhs, const Vector3CurveKeyData& rhs) { return lhs.time < rhs.time; });

        for (Vector3CurveKeyData& key : value.keys)
            key.time = clamp(key.time, 0.f, 1.f);

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            Vector3CurveKeyData& key = value.keys[index];
            const Vector3CurveKeyData* prevKey = index > 0 ? &value.keys[index - 1] : nullptr;
            const Vector3CurveKeyData* nextKey = index + 1 < value.keys.size() ? &value.keys[index + 1] : nullptr;

            if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            {
                const Vec3 tangent = Evaluate_Vector3CurveAutoClampedTangent(prevKey, key, nextKey);
                key.arriveTangent = tangent;
                key.leaveTangent = tangent;
            }
        }
    }

    static Color Evaluate_ColorCurveAutoClampedTangent(
        const ColorCurveKeyData* prevKey,
        const ColorCurveKeyData& key,
        const ColorCurveKeyData* nextKey)
    {
        if (prevKey == nullptr || nextKey == nullptr)
            return Color{};

        const float dt = nextKey->time - prevKey->time;
        if (dt <= 0.f)
            return Color{};

        return Color{
            (nextKey->value.x - prevKey->value.x) / dt,
            (nextKey->value.y - prevKey->value.y) / dt,
            (nextKey->value.z - prevKey->value.z) / dt,
            0.f
        };
    }

    static void Normalize_ColorCurveKeys(ConstantCurveColorDistributionData& value)
    {
        if (value.keys.empty())
        {
            value.keys = {
                ColorCurveKeyData{ 0.f, Color{ 1.f, 1.f, 1.f, 1.f }, Color{}, Color{}, FloatCurveInterpolationMode::Linear },
                ColorCurveKeyData{ 1.f, Color{ 1.f, 1.f, 1.f, 1.f }, Color{}, Color{}, FloatCurveInterpolationMode::Linear }
            };
            return;
        }

        ranges::sort(value.keys, [](const ColorCurveKeyData& lhs, const ColorCurveKeyData& rhs) { return lhs.time < rhs.time; });

        for (ColorCurveKeyData& key : value.keys)
        {
            key.time = clamp(key.time, 0.f, 1.f);
            key.value.w = 1.f;
            key.arriveTangent.w = 0.f;
            key.leaveTangent.w = 0.f;
        }

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            ColorCurveKeyData& key = value.keys[index];
            const ColorCurveKeyData* prevKey = index > 0 ? &value.keys[index - 1] : nullptr;
            const ColorCurveKeyData* nextKey = index + 1 < value.keys.size() ? &value.keys[index + 1] : nullptr;

            if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            {
                const Color tangent = Evaluate_ColorCurveAutoClampedTangent(prevKey, key, nextKey);
                key.arriveTangent = tangent;
                key.leaveTangent = tangent;
            }
        }
    }

    enum class CurveKeyHeaderAction
    {
        None,
        MoveUp,
        MoveDown,
        Remove,
    };

    template <typename TKey>
    static float Compute_NewCurveKeyTime(const vector<TKey>& keys)
    {
        if (keys.empty())
            return 0.f;

        return 1.f;
    }

    CurveKeyHeaderAction Draw_CurveKeyHeaderActions(size_t index, size_t keyCount)
    {
        CurveKeyHeaderAction action = CurveKeyHeaderAction::None;
        const bool canMoveUp = index > 0;
        const bool canMoveDown = index + 1 < keyCount;
        const bool canRemove = keyCount > 1;

        auto draw_action_button = [&](const char* label, const char* tooltip, bool enabled, CurveKeyHeaderAction buttonAction)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            if (ImGui::SmallButton(label) && enabled)
                action = buttonAction;

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", tooltip);

            if (!enabled)
                ImGui::EndDisabled();
        };

        draw_action_button(ICON_FA_ARROW_UP "##MoveUp", "위로 이동", canMoveUp, CurveKeyHeaderAction::MoveUp);
        ImGui::SameLine();
        draw_action_button(ICON_FA_ARROW_DOWN "##MoveDown", "아래로 이동", canMoveDown, CurveKeyHeaderAction::MoveDown);
        ImGui::SameLine();
        draw_action_button(ICON_FA_TRASH "##Remove", "인덱스 삭제", canRemove, CurveKeyHeaderAction::Remove);

        return action;
    }

    template <typename TKey>
    static bool Apply_CurveKeyHeaderAction(vector<TKey>& keys, size_t index, CurveKeyHeaderAction action)
    {
        switch (action)
        {
        case CurveKeyHeaderAction::MoveUp:
            if (index > 0 && index < keys.size())
            {
                swap(keys[index].time, keys[index - 1].time);
                return true;
            }
            break;

        case CurveKeyHeaderAction::MoveDown:
            if (index + 1 < keys.size())
            {
                swap(keys[index].time, keys[index + 1].time);
                return true;
            }
            break;

        case CurveKeyHeaderAction::Remove:
            if (keys.size() > 1 && index < keys.size())
            {
                keys.erase(keys.begin() + static_cast<vector<TKey>::difference_type>(index));
                return true;
            }
            break;

        case CurveKeyHeaderAction::None:
        default:
            break;
        }

        return false;
    }

    bool Draw_FloatCurveKeyListProperty(
        ConstantCurveFloatDistributionData& value,
        const ConstantCurveFloatDistributionData& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr)
    {
        bool changed = false;

        ImGui::PushID("PointsRow");
        _basicDrawer.Draw_PropertyLabel("포인트");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("키: %zu / %zu", value.keys.size(), kDistributionCurveMaxKeys);
        ImGui::SameLine();
        const bool canAddKey = value.keys.size() < kDistributionCurveMaxKeys;
        if (!canAddKey)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_PLUS "##AddFloatCurveKey"))
        {
            FloatCurveKeyData seed = value.keys.empty() ? FloatCurveKeyData{} : value.keys.back();
            seed.time = Compute_NewCurveKeyTime(value.keys);
            seed.value = Apply_Range(seed.value, range);
            value.keys.push_back(seed);
            changed = true;
        }
        if (!canAddKey)
            ImGui::EndDisabled();
        if (Draw_CurveBulkInterpolationControl(value.keys))
        {
            Normalize_FloatCurveKeys(value);
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        for (size_t index = 0; index < value.keys.size();)
        {
            ImGui::PushID(static_cast<int>(index));
            FloatCurveKeyData& key = value.keys[index];
            const FloatCurveKeyData defaultKey = index < defaultValue.keys.size() ? defaultValue.keys[index] : key;

            if (index > 0)
                ImGui::Dummy(ImVec2{ 0.f, 4.f });

            const string rowId = "인덱스 [" + to_string(index) + "]";
            _basicDrawer.Push_RowTint(Get_DistributionKeyHeaderRowTint());
            _basicDrawer.Push_LabelTint(Get_DistributionKeyLabelTint());
            const bool isOpen = _basicDrawer.Draw_TreePropertyLabel(rowId.c_str());
            const CurveKeyHeaderAction headerAction = Draw_CurveKeyHeaderActions(index, value.keys.size());
            _basicDrawer.Pop_LabelTint();
            if (Apply_CurveKeyHeaderAction(value.keys, index, headerAction))
            {
                changed = true;
                if (isOpen)
                    ImGui::TreePop();
                _basicDrawer.Pop_RowTint();
                ImGui::PopID();
                if (headerAction == CurveKeyHeaderAction::Remove)
                    continue;

                ++index;
                continue;
            }
            if (isOpen)
            {
                _basicDrawer.Push_RowTint(Get_DistributionKeyBodyRowTint());
                changed |= _basicDrawer.Draw_FloatProperty("In 값", key.time, defaultKey.time, speed);
                changed |= _basicDrawer.Draw_FloatProperty("Out 값", key.value, defaultKey.value, speed, nullptr, range);

                _basicDrawer.Draw_PropertyLabel("보간 모드");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##Interpolation", Get_FloatCurveInterpolationLabel(key.interpolationMode)))
                {
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Linear, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Constant, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAutoClamped, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAuto, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveUser, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveBreak, false, key.interpolationMode, changed);
                    ImGui::EndCombo();
                }
                ImGui::PushID("InterpolationReset");
                _basicDrawer.Draw_ResetButton(false);
                ImGui::PopID();

                if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
                {
                    _basicDrawer.Draw_PropertyLabel("고급");
                    ImGui::TextDisabled("Curve Auto Clamped");
                    _basicDrawer.Draw_ResetButton(false);
                    changed |= _basicDrawer.Draw_FloatProperty("도착 탄젠트", key.arriveTangent, defaultKey.arriveTangent, speed);
                    changed |= _basicDrawer.Draw_FloatProperty("출발 탄젠트", key.leaveTangent, defaultKey.leaveTangent, speed);
                }

                _basicDrawer.Pop_RowTint();
                ImGui::TreePop();
            }
            _basicDrawer.Pop_RowTint();
            ImGui::PopID();
            ++index;
        }

        return changed;
    }

    const char* Get_Vector3DistributionModeLabel(
        DistributionMode mode,
        bool showConstantCurveAsUnimplemented)
    {
        if (showConstantCurveAsUnimplemented && mode == DistributionMode::ConstantCurve)
            return "Vec3 고정값 커브 (미구현)";

        return Get_DistributionModeLabel(DistributionValueKind::Vec3, mode);
    }

    bool Draw_Vector2CurveKeyListProperty(
        ConstantCurveVector2DistributionData& value,
        const ConstantCurveVector2DistributionData& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr)
    {
        bool changed = false;

        ImGui::PushID("PointsRow");
        _basicDrawer.Draw_PropertyLabel("포인트");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("키: %zu / %zu", value.keys.size(), kDistributionCurveMaxKeys);
        ImGui::SameLine();
        const bool canAddKey = value.keys.size() < kDistributionCurveMaxKeys;
        if (!canAddKey)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_PLUS "##AddVector2CurveKey"))
        {
            Vector2CurveKeyData seed = value.keys.empty() ? Vector2CurveKeyData{} : value.keys.back();
            seed.time = Compute_NewCurveKeyTime(value.keys);
            seed.value = Apply_Range(seed.value, range);
            value.keys.push_back(seed);
            changed = true;
        }
        if (!canAddKey)
            ImGui::EndDisabled();
        if (Draw_CurveBulkInterpolationControl(value.keys))
        {
            Normalize_Vector2CurveKeys(value);
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        for (size_t index = 0; index < value.keys.size();)
        {
            ImGui::PushID(static_cast<int>(index));
            Vector2CurveKeyData& key = value.keys[index];
            const Vector2CurveKeyData defaultKey = index < defaultValue.keys.size() ? defaultValue.keys[index] : key;

            if (index > 0)
                ImGui::Dummy(ImVec2{ 0.f, 4.f });

            const string rowId = "인덱스 [" + to_string(index) + "]";
            _basicDrawer.Push_RowTint(Get_DistributionKeyHeaderRowTint());
            _basicDrawer.Push_LabelTint(Get_DistributionKeyLabelTint());
            const bool isOpen = _basicDrawer.Draw_TreePropertyLabel(rowId.c_str());
            const CurveKeyHeaderAction headerAction = Draw_CurveKeyHeaderActions(index, value.keys.size());
            _basicDrawer.Pop_LabelTint();
            if (Apply_CurveKeyHeaderAction(value.keys, index, headerAction))
            {
                changed = true;
                if (isOpen)
                    ImGui::TreePop();
                _basicDrawer.Pop_RowTint();
                ImGui::PopID();
                if (headerAction == CurveKeyHeaderAction::Remove)
                    continue;

                ++index;
                continue;
            }
            if (isOpen)
            {
                _basicDrawer.Push_RowTint(Get_DistributionKeyBodyRowTint());
                changed |= _basicDrawer.Draw_FloatProperty("In 값", key.time, defaultKey.time, speed);
                changed |= _basicDrawer.Draw_Vec2Property("Out 값", key.value, defaultKey.value, speed, range);

                _basicDrawer.Draw_PropertyLabel("보간 모드");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##Interpolation", Get_FloatCurveInterpolationLabel(key.interpolationMode)))
                {
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Linear, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Constant, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAutoClamped, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAuto, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveUser, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveBreak, false, key.interpolationMode, changed);
                    ImGui::EndCombo();
                }
                ImGui::PushID("InterpolationReset");
                _basicDrawer.Draw_ResetButton(false);
                ImGui::PopID();

                if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
                {
                    _basicDrawer.Draw_PropertyLabel("고급");
                    ImGui::TextDisabled("Curve Auto Clamped");
                    _basicDrawer.Draw_ResetButton(false);
                    changed |= _basicDrawer.Draw_Vec2Property("도착 탄젠트", key.arriveTangent, defaultKey.arriveTangent, speed);
                    changed |= _basicDrawer.Draw_Vec2Property("출발 탄젠트", key.leaveTangent, defaultKey.leaveTangent, speed);
                }

                _basicDrawer.Pop_RowTint();
                ImGui::TreePop();
            }
            _basicDrawer.Pop_RowTint();
            ImGui::PopID();
            ++index;
        }

        return changed;
    }

    bool Draw_Vector3CurveKeyListProperty(
        ConstantCurveVector3DistributionData& value,
        const ConstantCurveVector3DistributionData& defaultValue,
        float speed = 0.01f,
        const AuthoringValueRange* range = nullptr)
    {
        bool changed = false;

        ImGui::PushID("PointsRow");
        _basicDrawer.Draw_PropertyLabel("포인트");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("키: %zu / %zu", value.keys.size(), kDistributionCurveMaxKeys);
        ImGui::SameLine();
        const bool canAddKey = value.keys.size() < kDistributionCurveMaxKeys;
        if (!canAddKey)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_PLUS "##AddVector3CurveKey"))
        {
            Vector3CurveKeyData seed = value.keys.empty() ? Vector3CurveKeyData{} : value.keys.back();
            seed.time = Compute_NewCurveKeyTime(value.keys);
            seed.value = Apply_Range(seed.value, range);
            value.keys.push_back(seed);
            changed = true;
        }
        if (!canAddKey)
            ImGui::EndDisabled();
        if (Draw_CurveBulkInterpolationControl(value.keys))
        {
            Normalize_Vector3CurveKeys(value);
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        for (size_t index = 0; index < value.keys.size();)
        {
            ImGui::PushID(static_cast<int>(index));
            Vector3CurveKeyData& key = value.keys[index];
            const Vector3CurveKeyData defaultKey = index < defaultValue.keys.size() ? defaultValue.keys[index] : key;

            if (index > 0)
                ImGui::Dummy(ImVec2{ 0.f, 4.f });

            const string rowId = "인덱스 [" + to_string(index) + "]";
            _basicDrawer.Push_RowTint(Get_DistributionKeyHeaderRowTint());
            _basicDrawer.Push_LabelTint(Get_DistributionKeyLabelTint());
            const bool isOpen = _basicDrawer.Draw_TreePropertyLabel(rowId.c_str());
            const CurveKeyHeaderAction headerAction = Draw_CurveKeyHeaderActions(index, value.keys.size());
            _basicDrawer.Pop_LabelTint();
            if (Apply_CurveKeyHeaderAction(value.keys, index, headerAction))
            {
                changed = true;
                if (isOpen)
                    ImGui::TreePop();
                _basicDrawer.Pop_RowTint();
                ImGui::PopID();
                if (headerAction == CurveKeyHeaderAction::Remove)
                    continue;

                ++index;
                continue;
            }
            if (isOpen)
            {
                _basicDrawer.Push_RowTint(Get_DistributionKeyBodyRowTint());
                changed |= _basicDrawer.Draw_FloatProperty("In 값", key.time, defaultKey.time, speed);
                changed |= _basicDrawer.Draw_Vec3Property("Out 값", key.value, defaultKey.value, speed, range);

                _basicDrawer.Draw_PropertyLabel("보간 모드");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##Interpolation", Get_FloatCurveInterpolationLabel(key.interpolationMode)))
                {
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Linear, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Constant, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAutoClamped, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAuto, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveUser, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveBreak, false, key.interpolationMode, changed);
                    ImGui::EndCombo();
                }
                ImGui::PushID("InterpolationReset");
                _basicDrawer.Draw_ResetButton(false);
                ImGui::PopID();

                if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
                {
                    _basicDrawer.Draw_PropertyLabel("고급");
                    ImGui::TextDisabled("Curve Auto Clamped");
                    _basicDrawer.Draw_ResetButton(false);
                    changed |= _basicDrawer.Draw_Vec3Property("도착 탄젠트", key.arriveTangent, defaultKey.arriveTangent, speed);
                    changed |= _basicDrawer.Draw_Vec3Property("출발 탄젠트", key.leaveTangent, defaultKey.leaveTangent, speed);
                }

                _basicDrawer.Pop_RowTint();
                ImGui::TreePop();
            }
            _basicDrawer.Pop_RowTint();
            ImGui::PopID();
            ++index;
        }

        return changed;
    }

    bool Draw_ColorCurveKeyListProperty(
        ConstantCurveColorDistributionData& value,
        const ConstantCurveColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID("PointsRow");
        _basicDrawer.Draw_PropertyLabel("포인트");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("키: %zu / %zu", value.keys.size(), kDistributionCurveMaxKeys);
        ImGui::SameLine();
        const bool canAddKey = value.keys.size() < kDistributionCurveMaxKeys;
        if (!canAddKey)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_PLUS "##AddColorCurveKey"))
        {
            ColorCurveKeyData seed = value.keys.empty() ? ColorCurveKeyData{} : value.keys.back();
            seed.time = Compute_NewCurveKeyTime(value.keys);
            value.keys.push_back(seed);
            changed = true;
        }
        if (!canAddKey)
            ImGui::EndDisabled();
        if (Draw_CurveBulkInterpolationControl(value.keys))
        {
            Normalize_ColorCurveKeys(value);
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        for (size_t index = 0; index < value.keys.size();)
        {
            ImGui::PushID(static_cast<int>(index));
            ColorCurveKeyData& key = value.keys[index];
            const ColorCurveKeyData defaultKey = index < defaultValue.keys.size() ? defaultValue.keys[index] : key;

            if (index > 0)
                ImGui::Dummy(ImVec2{ 0.f, 4.f });

            const string rowId = "인덱스 [" + to_string(index) + "]";
            _basicDrawer.Push_RowTint(Get_DistributionKeyHeaderRowTint());
            _basicDrawer.Push_LabelTint(Get_DistributionKeyLabelTint());
            const bool isOpen = _basicDrawer.Draw_TreePropertyLabel(rowId.c_str());
            const CurveKeyHeaderAction headerAction = Draw_CurveKeyHeaderActions(index, value.keys.size());
            _basicDrawer.Pop_LabelTint();
            if (Apply_CurveKeyHeaderAction(value.keys, index, headerAction))
            {
                changed = true;
                if (isOpen)
                    ImGui::TreePop();
                _basicDrawer.Pop_RowTint();
                ImGui::PopID();
                if (headerAction == CurveKeyHeaderAction::Remove)
                    continue;

                ++index;
                continue;
            }
            if (isOpen)
            {
                _basicDrawer.Push_RowTint(Get_DistributionKeyBodyRowTint());
                changed |= _basicDrawer.Draw_FloatProperty("In 값", key.time, defaultKey.time);
                changed |= _basicDrawer.Draw_ColorRgbProperty("Out RGB", key.value, defaultKey.value);

                _basicDrawer.Draw_PropertyLabel("보간 모드");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##Interpolation", Get_FloatCurveInterpolationLabel(key.interpolationMode)))
                {
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Linear, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Constant, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAutoClamped, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAuto, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveUser, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveBreak, false, key.interpolationMode, changed);
                    ImGui::EndCombo();
                }
                ImGui::PushID("InterpolationReset");
                _basicDrawer.Draw_ResetButton(false);
                ImGui::PopID();

                _basicDrawer.Pop_RowTint();
                ImGui::TreePop();
            }
            _basicDrawer.Pop_RowTint();
            ImGui::PopID();
            ++index;
        }

        return changed;
    }

    bool Draw_FloatDistributionProperty(
        const char* label,
        FloatDistributionData& value,
        const FloatDistributionData& defaultValue,
        float speed = 0.01f,
        bool allowUniform = true,
        bool allowConstantCurve = true,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label, tooltip);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Float, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();

                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Float, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();

                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, allowUniform);
            draw_selectable(DistributionMode::ConstantCurve, allowConstantCurve);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_FloatDistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantFloatDistributionData>(&defaultValue.payload);
                const float defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_FloatProperty("상수", constant->value, defaultField, speed, nullptr, range);
            }
            break;

        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformFloatDistributionData>(&defaultValue.payload);
                const float defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const float defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_FloatProperty("최소", uniform->minValue, defaultMin, speed, nullptr, range);
                changed |= _basicDrawer.Draw_FloatProperty("최대", uniform->maxValue, defaultMax, speed, nullptr, range);
                Normalize_FloatUniform(*uniform);
                changed |= Draw_RandomSeedPayload(uniform->randomSeed);
            }
            break;

        case DistributionMode::ConstantCurve:
            if (allowConstantCurve)
            {
                if (auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
                {
                    const auto* defaultCurve = get_if<ConstantCurveFloatDistributionData>(&defaultValue.payload);
                    const FloatDistributionData fallbackCurveData = FloatDistributionData::Make_ConstantCurve(0.f);
                    const auto* fallbackDefault = get_if<ConstantCurveFloatDistributionData>(&fallbackCurveData.payload);
                    changed |= Draw_FloatCurveKeyListProperty(*curve, defaultCurve != nullptr ? *defaultCurve : *fallbackDefault, speed, range);
                    Normalize_FloatCurveKeys(*curve);
                }
            }
            else
                ImGui::TextDisabled("이 분포에서는 미구현");
            break;

        case DistributionMode::UniformCurve:
        case DistributionMode::ParticleParameter:
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_Vector2DistributionProperty(
        const char* label,
        Vector2DistributionData& value,
        const Vector2DistributionData& defaultValue,
        float speed = 0.01f,
        bool allowConstantCurve = true,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label, tooltip);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Vec2, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Vec2, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, allowConstantCurve);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_Vector2DistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantVector2DistributionData>(&defaultValue.payload);
                const Vec2 defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_Vec2Property("상수", constant->value, defaultField, speed, range);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformVector2DistributionData>(&defaultValue.payload);
                const Vec2 defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Vec2 defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_Vec2Property("최소", uniform->minValue, defaultMin, speed, range);
                changed |= _basicDrawer.Draw_Vec2Property("최대", uniform->maxValue, defaultMax, speed, range);
                Normalize_Vector2Uniform(*uniform);
                changed |= Draw_RandomSeedPayload(uniform->randomSeed);
            }
            break;
        case DistributionMode::ConstantCurve:
            if (allowConstantCurve)
            {
                if (auto* curve = get_if<ConstantCurveVector2DistributionData>(&value.payload))
                {
                    const auto* defaultCurve = get_if<ConstantCurveVector2DistributionData>(&defaultValue.payload);
                    const Vector2DistributionData fallbackCurveData = Vector2DistributionData::Make_ConstantCurve(Vec2{ 0.f, 0.f });
                    const auto* fallbackDefault = get_if<ConstantCurveVector2DistributionData>(&fallbackCurveData.payload);
                    changed |= Draw_Vector2CurveKeyListProperty(*curve, defaultCurve != nullptr ? *defaultCurve : *fallbackDefault, speed, range);
                    Normalize_Vector2CurveKeys(*curve);
                }
            }
            else
                ImGui::TextDisabled("이 분포에서는 미구현");
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_Vector3DistributionProperty(
        const char* label,
        Vector3DistributionData& value,
        const Vector3DistributionData& defaultValue,
        float speed = 0.01f,
        bool allowConstantCurve = true,
        bool showConstantCurveAsUnimplemented = false,
        const AuthoringValueRange* range = nullptr,
        const char* tooltip = nullptr)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label, tooltip);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_Vector3DistributionModeLabel(nextMode, showConstantCurveAsUnimplemented)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_Vector3DistributionModeLabel(mode, showConstantCurveAsUnimplemented), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, allowConstantCurve);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_Vector3DistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantVector3DistributionData>(&defaultValue.payload);
                const Vec3 defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_Vec3Property("상수", constant->value, defaultField, speed, range);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformVector3DistributionData>(&defaultValue.payload);
                const Vec3 defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Vec3 defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_Vec3Property("최소", uniform->minValue, defaultMin, speed, range);
                changed |= _basicDrawer.Draw_Vec3Property("최대", uniform->maxValue, defaultMax, speed, range);
                Normalize_Vector3Uniform(*uniform);
                changed |= Draw_RandomSeedPayload(uniform->randomSeed);
            }
            break;
        case DistributionMode::ConstantCurve:
            if (allowConstantCurve)
            {
                if (auto* curve = get_if<ConstantCurveVector3DistributionData>(&value.payload))
                {
                    const auto* defaultCurve = get_if<ConstantCurveVector3DistributionData>(&defaultValue.payload);
                    const Vector3DistributionData fallbackCurveData = Vector3DistributionData::Make_ConstantCurve(Vec3{ 0.f, 0.f, 0.f });
                    const auto* fallbackDefault = get_if<ConstantCurveVector3DistributionData>(&fallbackCurveData.payload);
                    changed |= Draw_Vector3CurveKeyListProperty(*curve, defaultCurve != nullptr ? *defaultCurve : *fallbackDefault, speed, range);
                    Normalize_Vector3CurveKeys(*curve);
                }
            }
            else
                ImGui::TextDisabled("이 모듈에서는 미구현");
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorDistributionProperty(
        const char* label,
        ColorDistributionData& value,
        const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Color, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Color, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, true);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_ColorDistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantColorDistributionData>(&defaultValue.payload);
                const Color defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_ColorProperty("상수", constant->value, defaultField);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformColorDistributionData>(&defaultValue.payload);
                const Color defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Color defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_ColorProperty("최소", uniform->minValue, defaultMin);
                changed |= _basicDrawer.Draw_ColorProperty("최대", uniform->maxValue, defaultMax);
                Normalize_ColorUniform(*uniform);
                changed |= Draw_RandomSeedPayload(uniform->randomSeed);
            }
            break;
        case DistributionMode::ConstantCurve:
            if (auto* curve = get_if<ConstantCurveColorDistributionData>(&value.payload))
            {
                const auto* defaultCurve = get_if<ConstantCurveColorDistributionData>(&defaultValue.payload);
                const ColorDistributionData fallbackCurveData = ColorDistributionData::Make_ConstantCurve(Color{ 1.f, 1.f, 1.f, 1.f });
                const auto* fallbackDefault = get_if<ConstantCurveColorDistributionData>(&fallbackCurveData.payload);
                changed |= Draw_ColorCurveKeyListProperty(*curve, defaultCurve != nullptr ? *defaultCurve : *fallbackDefault);
                Normalize_ColorCurveKeys(*curve);
            }
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorRgbDistributionProperty(
        const char* label,
        ColorDistributionData& value,
        const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Color, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Color, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, true);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_ColorDistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantColorDistributionData>(&defaultValue.payload);
                const Color defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_ColorRgbProperty("상수 RGB", constant->value, defaultField);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformColorDistributionData>(&defaultValue.payload);
                const Color defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Color defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_ColorRgbProperty("최소 RGB", uniform->minValue, defaultMin);
                changed |= _basicDrawer.Draw_ColorRgbProperty("최대 RGB", uniform->maxValue, defaultMax);
                Normalize_ColorUniform(*uniform);
                changed |= Draw_RandomSeedPayload(uniform->randomSeed);
            }
            break;
        case DistributionMode::ConstantCurve:
            if (auto* curve = get_if<ConstantCurveColorDistributionData>(&value.payload))
            {
                const auto* defaultCurve = get_if<ConstantCurveColorDistributionData>(&defaultValue.payload);
                const ColorDistributionData fallbackCurveData = ColorDistributionData::Make_ConstantCurve(Color{ 1.f, 1.f, 1.f, 1.f });
                const auto* fallbackDefault = get_if<ConstantCurveColorDistributionData>(&fallbackCurveData.payload);
                changed |= Draw_ColorCurveKeyListProperty(*curve, defaultCurve != nullptr ? *defaultCurve : *fallbackDefault);
                Normalize_ColorCurveKeys(*curve);
            }
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    static bool Is_DefaultValue(float value, float defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(const Vec2& value, const Vec2& defaultValue) { return value.x == defaultValue.x && value.y == defaultValue.y; }

    static bool Is_DefaultValue(const Vec3& value, const Vec3& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z;
    }

    static bool Is_DefaultValue(const Color& value, const Color& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z && value.w == defaultValue.w;
    }

    static bool Is_DefaultValue(const DistributionRandomSeedData& value, const DistributionRandomSeedData& defaultValue)
    {
        return value.mode == defaultValue.mode &&
               value.manualSeed == defaultValue.manualSeed &&
               value.useInstanceSeed == defaultValue.useInstanceSeed;
    }

    static bool Is_DefaultValue(const ConstantFloatDistributionData& value, const ConstantFloatDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformFloatDistributionData& value, const UniformFloatDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue) &&
               Is_DefaultValue(value.randomSeed, defaultValue.randomSeed);
    }

    static bool Is_DefaultValue(const FloatCurveKeyData& value, const FloatCurveKeyData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) &&
               Is_DefaultValue(value.value, defaultValue.value) &&
               Is_DefaultValue(value.arriveTangent, defaultValue.arriveTangent) &&
               Is_DefaultValue(value.leaveTangent, defaultValue.leaveTangent) &&
               value.interpolationMode == defaultValue.interpolationMode;
    }

    static bool Is_DefaultValue(const ConstantCurveFloatDistributionData& value, const ConstantCurveFloatDistributionData& defaultValue)
    {
        if (value.keys.size() != defaultValue.keys.size())
            return false;

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            if (!Is_DefaultValue(value.keys[index], defaultValue.keys[index]))
                return false;
        }

        return true;
    }

    static bool Is_DefaultValue(const FloatDistributionData& value, const FloatDistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;

        if (const auto* current = get_if<ConstantFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantFloatDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformFloatDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<ConstantCurveFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantCurveFloatDistributionData>(defaultValue.payload));
        return false;
    }

    static bool Is_DefaultValue(const ConstantVector2DistributionData& value, const ConstantVector2DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformVector2DistributionData& value, const UniformVector2DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue) &&
               Is_DefaultValue(value.randomSeed, defaultValue.randomSeed);
    }

    static bool Is_DefaultValue(const Vector2CurveKeyData& value, const Vector2CurveKeyData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) &&
               Is_DefaultValue(value.value, defaultValue.value) &&
               Is_DefaultValue(value.arriveTangent, defaultValue.arriveTangent) &&
               Is_DefaultValue(value.leaveTangent, defaultValue.leaveTangent) &&
               value.interpolationMode == defaultValue.interpolationMode;
    }

    static bool Is_DefaultValue(const ConstantCurveVector2DistributionData& value, const ConstantCurveVector2DistributionData& defaultValue)
    {
        if (value.keys.size() != defaultValue.keys.size())
            return false;

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            if (!Is_DefaultValue(value.keys[index], defaultValue.keys[index]))
                return false;
        }

        return true;
    }

    static bool Is_DefaultValue(const Vector2DistributionData& value, const Vector2DistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;
        if (const auto* current = get_if<ConstantVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantVector2DistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformVector2DistributionData>(defaultValue.payload));
        if (const auto* current = get_if<ConstantCurveVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantCurveVector2DistributionData>(defaultValue.payload));
        return false;
    }

    static bool Is_DefaultValue(const ConstantVector3DistributionData& value, const ConstantVector3DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformVector3DistributionData& value, const UniformVector3DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue) &&
               Is_DefaultValue(value.randomSeed, defaultValue.randomSeed);
    }

    static bool Is_DefaultValue(const Vector3DistributionData& value, const Vector3DistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;
        if (const auto* current = get_if<ConstantVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantVector3DistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformVector3DistributionData>(defaultValue.payload));
        if (const auto* current = get_if<ConstantCurveVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantCurveVector3DistributionData>(defaultValue.payload));
        return false;
    }

    static bool Is_DefaultValue(const ConstantColorDistributionData& value, const ConstantColorDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformColorDistributionData& value, const UniformColorDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue) &&
               Is_DefaultValue(value.randomSeed, defaultValue.randomSeed);
    }

    static bool Is_DefaultValue(const Vector3CurveKeyData& value, const Vector3CurveKeyData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) &&
               Is_DefaultValue(value.value, defaultValue.value) &&
               Is_DefaultValue(value.arriveTangent, defaultValue.arriveTangent) &&
               Is_DefaultValue(value.leaveTangent, defaultValue.leaveTangent) &&
               value.interpolationMode == defaultValue.interpolationMode;
    }

    static bool Is_DefaultValue(const ConstantCurveVector3DistributionData& value, const ConstantCurveVector3DistributionData& defaultValue)
    {
        if (value.keys.size() != defaultValue.keys.size())
            return false;

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            if (!Is_DefaultValue(value.keys[index], defaultValue.keys[index]))
                return false;
        }

        return true;
    }

    static bool Is_DefaultValue(const ColorCurveKeyData& value, const ColorCurveKeyData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) &&
               Is_DefaultValue(value.value, defaultValue.value) &&
               Is_DefaultValue(value.arriveTangent, defaultValue.arriveTangent) &&
               Is_DefaultValue(value.leaveTangent, defaultValue.leaveTangent) &&
               value.interpolationMode == defaultValue.interpolationMode;
    }

    static bool Is_DefaultValue(const ConstantCurveColorDistributionData& value, const ConstantCurveColorDistributionData& defaultValue)
    {
        if (value.keys.size() != defaultValue.keys.size())
            return false;

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            if (!Is_DefaultValue(value.keys[index], defaultValue.keys[index]))
                return false;
        }

        return true;
    }

    static bool Is_DefaultValue(const ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;
        if (const auto* current = get_if<ConstantColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantColorDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformColorDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<ConstantCurveColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantCurveColorDistributionData>(defaultValue.payload));
        return false;
    }

    static void Transition_FloatDistributionMode(FloatDistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
            return;

        switch (nextMode)
        {
        case DistributionMode::Constant:
            if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
                value.payload = ConstantFloatDistributionData{ uniform->minValue };
            else if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
                value.payload = ConstantFloatDistributionData{ constant->value };
            else if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
                value.payload = ConstantFloatDistributionData{ curve->keys.empty() ? 0.f : curve->keys.front().value };
            else
                value.payload = ConstantFloatDistributionData{};
            break;

        case DistributionMode::Uniform:
            if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
                value.payload = UniformFloatDistributionData{ constant->value, constant->value };
            else if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
                value.payload = *uniform;
            else if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
            {
                const float minValue = curve->keys.empty() ? 0.f : curve->keys.front().value;
                const float maxValue = curve->keys.size() > 1 ? curve->keys.back().value : minValue;
                value.payload = UniformFloatDistributionData{ minValue, maxValue };
            }
            else
                value.payload = UniformFloatDistributionData{};
            break;

        case DistributionMode::ConstantCurve:
            if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
                value.payload = FloatDistributionData::Make_ConstantCurve(constant->value).payload;
            else if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
                value.payload = FloatDistributionData::Make_ConstantCurve(uniform->minValue).payload;
            else if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
                value.payload = *curve;
            else
                value.payload = FloatDistributionData::Make_ConstantCurve(0.f).payload;
            break;

        default:
            return;
        }

        value.mode = nextMode;
    }

    static void Transition_Vector2DistributionMode(Vector2DistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
            return;

        switch (nextMode)
        {
        case DistributionMode::Constant:
            if (const auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
                value.payload = ConstantVector2DistributionData{ uniform->minValue };
            else if (const auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
                value.payload = ConstantVector2DistributionData{ constant->value };
            else if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&value.payload))
                value.payload = ConstantVector2DistributionData{ curve->keys.empty() ? Vec2{} : curve->keys.front().value };
            else
                value.payload = ConstantVector2DistributionData{};
            break;
        case DistributionMode::Uniform:
            if (const auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
                value.payload = UniformVector2DistributionData{ constant->value, constant->value };
            else if (const auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
                value.payload = *uniform;
            else if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&value.payload))
            {
                const Vec2 minValue = curve->keys.empty() ? Vec2{} : curve->keys.front().value;
                const Vec2 maxValue = curve->keys.size() > 1 ? curve->keys.back().value : minValue;
                value.payload = UniformVector2DistributionData{ minValue, maxValue };
            }
            else
                value.payload = UniformVector2DistributionData{};
            break;
        case DistributionMode::ConstantCurve:
            if (const auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
                value.payload = Vector2DistributionData::Make_ConstantCurve(constant->value).payload;
            else if (const auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
                value.payload = Vector2DistributionData::Make_ConstantCurve(uniform->minValue, uniform->maxValue).payload;
            else if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&value.payload))
                value.payload = *curve;
            else
                value.payload = Vector2DistributionData::Make_ConstantCurve(Vec2{ 0.f, 0.f }).payload;
            break;
        default:
            return;
        }

        value.mode = nextMode;
    }

    static void Transition_Vector3DistributionMode(Vector3DistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
            return;

        switch (nextMode)
        {
        case DistributionMode::Constant:
            if (const auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
                value.payload = ConstantVector3DistributionData{ uniform->minValue };
            else if (const auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
                value.payload = ConstantVector3DistributionData{ constant->value };
            else if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&value.payload))
                value.payload = ConstantVector3DistributionData{ curve->keys.empty() ? Vec3{} : curve->keys.front().value };
            else
                value.payload = ConstantVector3DistributionData{};
            break;
        case DistributionMode::Uniform:
            if (const auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
                value.payload = UniformVector3DistributionData{ constant->value, constant->value };
            else if (const auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
                value.payload = *uniform;
            else if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&value.payload))
            {
                const Vec3 minValue = curve->keys.empty() ? Vec3{} : curve->keys.front().value;
                const Vec3 maxValue = curve->keys.size() > 1 ? curve->keys.back().value : minValue;
                value.payload = UniformVector3DistributionData{ minValue, maxValue };
            }
            else
                value.payload = UniformVector3DistributionData{};
            break;
        case DistributionMode::ConstantCurve:
            if (const auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
                value.payload = Vector3DistributionData::Make_ConstantCurve(constant->value).payload;
            else if (const auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
                value.payload = Vector3DistributionData::Make_ConstantCurve(uniform->minValue, uniform->maxValue).payload;
            else if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&value.payload))
                value.payload = *curve;
            else
                value.payload = Vector3DistributionData::Make_ConstantCurve(Vec3{ 0.f, 0.f, 0.f }).payload;
            break;
        default:
            return;
        }

        value.mode = nextMode;
    }

    static void Transition_ColorDistributionMode(ColorDistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
            return;

        switch (nextMode)
        {
        case DistributionMode::Constant:
            if (const auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
                value.payload = ConstantColorDistributionData{ uniform->minValue };
            else if (const auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
                value.payload = ConstantColorDistributionData{ constant->value };
            else if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&value.payload))
                value.payload = ConstantColorDistributionData{ curve->keys.empty() ? Color{ 1.f, 1.f, 1.f, 1.f } : curve->keys.front().value };
            else
                value.payload = ConstantColorDistributionData{};
            break;
        case DistributionMode::Uniform:
            if (const auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
                value.payload = UniformColorDistributionData{ constant->value, constant->value };
            else if (const auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
                value.payload = *uniform;
            else if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&value.payload))
            {
                const Color minValue = curve->keys.empty() ? Color{ 1.f, 1.f, 1.f, 1.f } : curve->keys.front().value;
                const Color maxValue = curve->keys.size() > 1 ? curve->keys.back().value : minValue;
                value.payload = UniformColorDistributionData{ minValue, maxValue };
            }
            else
                value.payload = UniformColorDistributionData{};
            break;
        case DistributionMode::ConstantCurve:
            if (const auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
                value.payload = ColorDistributionData::Make_ConstantCurve(constant->value).payload;
            else if (const auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
                value.payload = ColorDistributionData::Make_ConstantCurve(uniform->minValue, uniform->maxValue).payload;
            else if (const auto* curve = get_if<ConstantCurveColorDistributionData>(&value.payload))
                value.payload = *curve;
            else
                value.payload = ColorDistributionData::Make_ConstantCurve(Color{ 1.f, 1.f, 1.f, 1.f }).payload;
            break;
        default:
            return;
        }

        value.mode = nextMode;
    }
};

DetailPropertyContext::DetailPropertyContext()
    : _basicDrawer{ make_unique<BasicDetailPropertyDrawer>() }
    , _distributionDrawer{ make_unique<DistributionDetailPropertyDrawer>(*_basicDrawer) }
{
}

DetailPropertyContext::~DetailPropertyContext() = default;

bool DetailPropertyContext::Begin_PropertyTable(const char* id)
{
    return _basicDrawer->Begin_PropertyTable(id);
}

void DetailPropertyContext::End_PropertyTable()
{
    _basicDrawer->End_PropertyTable();
}

void DetailPropertyContext::Draw_PropertyLabel(const char* label, const char* tooltip)
{
    _basicDrawer->Draw_PropertyLabel(label, tooltip);
}

bool DetailPropertyContext::Draw_ResetButton(bool canReset)
{
    return _basicDrawer->Draw_ResetButton(canReset);
}

bool DetailPropertyContext::Draw_StringProperty(const char* label, string& value, const string& defaultValue, const char* tooltip)
{
    return _basicDrawer->Draw_StringProperty(label, value, defaultValue, tooltip);
}

bool DetailPropertyContext::Draw_BoolProperty(const char* label, bool& value, bool defaultValue, const char* tooltip)
{
    return _basicDrawer->Draw_BoolProperty(label, value, defaultValue, tooltip);
}

bool DetailPropertyContext::Draw_FloatProperty(
    const char* label,
    float& value,
    float defaultValue,
    float speed,
    const char* tooltip,
    const AuthoringValueRange* range)
{
    return _basicDrawer->Draw_FloatProperty(label, value, defaultValue, speed, tooltip, range);
}

bool DetailPropertyContext::Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue, const char* tooltip)
{
    return _basicDrawer->Draw_ColorProperty(label, value, defaultValue, tooltip);
}

bool DetailPropertyContext::Draw_ColorRgbProperty(const char* label, Color& value, const Color& defaultValue, const char* tooltip)
{
    return _basicDrawer->Draw_ColorRgbProperty(label, value, defaultValue, tooltip);
}

bool DetailPropertyContext::Draw_FloatDistributionGroup(
    const char* label,
    FloatDistributionData& value,
    const FloatDistributionData& defaultValue,
    float speed,
    bool allowUniform,
    bool allowConstantCurve,
    const AuthoringValueRange* range,
    const char* tooltip)
{
    return _distributionDrawer->Draw_FloatDistributionGroup(label, value, defaultValue, speed, allowUniform, allowConstantCurve, range, tooltip);
}

bool DetailPropertyContext::Draw_Vector2DistributionGroup(
    const char* label,
    Vector2DistributionData& value,
    const Vector2DistributionData& defaultValue,
    float speed,
    bool allowConstantCurve,
    const AuthoringValueRange* range,
    const char* tooltip)
{
    return _distributionDrawer->Draw_Vector2DistributionGroup(label, value, defaultValue, speed, allowConstantCurve, range, tooltip);
}

bool DetailPropertyContext::Draw_Vector3DistributionGroup(
    const char* label,
    Vector3DistributionData& value,
    const Vector3DistributionData& defaultValue,
    float speed,
    bool allowConstantCurve,
    bool showConstantCurveAsUnimplemented,
    const AuthoringValueRange* range,
    const char* tooltip)
{
    return _distributionDrawer->Draw_Vector3DistributionGroup(
        label,
        value,
        defaultValue,
        speed,
        allowConstantCurve,
        showConstantCurveAsUnimplemented,
        range,
        tooltip
    );
}

bool DetailPropertyContext::Draw_ColorDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
{
    return _distributionDrawer->Draw_ColorDistributionGroup(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_ColorRgbDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
{
    return _distributionDrawer->Draw_ColorRgbDistributionGroup(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_BurstListProperty(
    vector<SpawnModuleData::ParticleBurstData>& burstList,
    const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
{
    return _basicDrawer->Draw_BurstListProperty(burstList, defaultValue);
}

bool DetailPropertyContext::Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
{
    return _basicDrawer->Draw_ScreenAlignmentProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_SourceHistorySpriteTrailScreenAlignmentProperty(
    const char* label,
    EmitterScreenAlignment& value,
    EmitterScreenAlignment defaultValue)
{
    return _basicDrawer->Draw_SourceHistorySpriteTrailScreenAlignmentProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_DirectionalAlignmentModeProperty(
    const char* label,
    EmitterDirectionalAlignmentMode& outValue,
    EmitterDirectionalAlignmentMode defaultValue)
{
    int currentIndex = outValue == EmitterDirectionalAlignmentMode::LookDirection ? 0 : 1;
    static constexpr const char* modeLabels[] = { "Look Direction", "Texture Axis" };

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = false;

    if (ImGui::BeginCombo("##Value", modeLabels[currentIndex]))
    {
        for (int index = 0; index < IM_ARRAYSIZE(modeLabels); ++index)
        {
            const bool isSelected = currentIndex == index;
            if (ImGui::Selectable(modeLabels[index], isSelected))
            {
                currentIndex = index;
                changed = true;
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip(
                    index == 0
                    ? "기준 방향을 sprite 카드의 normal/look 방향으로 사용합니다."
                    : "texture의 X/Y 축을 기준 방향에 맞추고, 나머지 축은 카메라 가시성을 유지합니다."
                );
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    const bool resetClicked = Draw_ResetButton(outValue != defaultValue);
    ImGui::PopID();

    if (resetClicked)
    {
        outValue = defaultValue;
        return true;
    }

    if (!changed)
        return false;

    outValue = currentIndex == 0
               ? EmitterDirectionalAlignmentMode::LookDirection
               : EmitterDirectionalAlignmentMode::TextureAxis;

    return true;
}

bool DetailPropertyContext::Draw_SpriteTextureAxisProperty(
    const char* label,
    EmitterSpriteTextureAxis& outValue,
    EmitterSpriteTextureAxis defaultValue,
    const char* textureXAxisTooltip,
    const char* textureYAxisTooltip)
{
    int currentIndex = outValue == EmitterSpriteTextureAxis::X ? 0 : 1;
    static constexpr const char* kTextureAxisLabels[] = {
        "Texture X",
        "Texture Y",
    };

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = false;
    if (ImGui::BeginCombo("##Value", kTextureAxisLabels[currentIndex]))
    {
        for (int index = 0; index < IM_ARRAYSIZE(kTextureAxisLabels); ++index)
        {
            const bool isSelected = currentIndex == index;
            if (ImGui::Selectable(kTextureAxisLabels[index], isSelected))
            {
                currentIndex = index;
                changed = true;
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip(
                    "%s",
                    index == 0
                    ? (textureXAxisTooltip != nullptr ? textureXAxisTooltip : "정렬 방향에 texture의 가로축을 맞춥니다.")
                    : textureYAxisTooltip != nullptr ? textureYAxisTooltip : "정렬 방향에 texture의 세로축을 맞춥니다. 세로로 긴 번개/연기 texture의 기본 후보입니다."
                );
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    const bool resetClicked = Draw_ResetButton(outValue != defaultValue);
    ImGui::PopID();

    if (resetClicked)
    {
        outValue = defaultValue;
        return true;
    }

    if (!changed)
        return false;

    outValue = currentIndex == 0 ? EmitterSpriteTextureAxis::X : EmitterSpriteTextureAxis::Y;
    return true;
}

void DetailPropertyContext::Draw_ScreenAlignmentImplementationNote(EmitterScreenAlignment value)
{
    _basicDrawer->Draw_ScreenAlignmentImplementationNote(value);
}

bool DetailPropertyContext::Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
{
    return _basicDrawer->Draw_SortModeProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue, const char* tooltip)
{
    return _basicDrawer->Draw_UintProperty(label, value, defaultValue, tooltip);
}

bool DetailPropertyContext::Draw_Vec2Property(
    const char* label,
    Vec2& value,
    const Vec2& defaultValue,
    float speed,
    const AuthoringValueRange* range,
    const char* tooltip)
{
    return _basicDrawer->Draw_Vec2Property(label, value, defaultValue, speed, range, tooltip);
}

bool DetailPropertyContext::Draw_Vec3Property(
    const char* label,
    Vec3& value,
    const Vec3& defaultValue,
    float speed,
    const AuthoringValueRange* range,
    const char* tooltip)
{
    return _basicDrawer->Draw_Vec3Property(label, value, defaultValue, speed, range, tooltip);
}

NS_END
