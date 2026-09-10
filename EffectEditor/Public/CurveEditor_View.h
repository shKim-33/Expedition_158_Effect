#pragma once

#include "Editor_Window.h"
#include "Emitter_View.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View;

class CurveEditor_View final : public Editor_Window
{
public:
    CurveEditor_View();
    ~CurveEditor_View() override = default;

public:
    void Render() override;
    void Pin_ModuleTargets(uint32 emitterId, uint32 moduleId);
    bool Convert_AndPin_ModuleTargets(uint32 emitterId, uint32 moduleId);
    void Clear_PinnedTracks();
    bool Is_ModuleFocused(uint32 emitterId, uint32 moduleId) const;
    static bool Has_ModuleCurveTargets(AuthoringEmitter& emitter, AuthoringModule& module);
    static bool Has_ModuleConvertibleCurveTargets(AuthoringEmitter& emitter, AuthoringModule& module);

private: //## Types::CurveTarget
    enum class CurveTargetChannel : uint8
    {
        Value,
        X, Y, Z,
        R, G, B,
    };

    enum class CurveTargetPayloadKind : uint8
    {
        Float,
        Vector2,
        Vector3,
        Color,
    };

    struct CurveTargetIdentity
    {
        uint32 emitterId{};
        uint32 moduleId{};
        AuthoringModuleType moduleType{ AuthoringModuleType::Required };
        string propertyId{};
        CurveTargetChannel channel{ CurveTargetChannel::Value };

        bool operator==(const CurveTargetIdentity& rhs) const = default;
    };

    struct PinnedTrack
    {
        CurveTargetIdentity identity{};
        bool visible{ true };
    };

    struct CurveTarget
    {
        CurveTargetIdentity identity{};
        AuthoringEmitter* emitter{};
        AuthoringModule* module{};
        variant<
            FloatDistributionData*,
            Vector2DistributionData*,
            Vector3DistributionData*,
            ColorDistributionData*> distribution{};
        string label{};
        string propertyLabel{};
        CurveTargetPayloadKind payloadKind{ CurveTargetPayloadKind::Float };
        CurveTargetChannel channel{ CurveTargetChannel::Value };
        ImU32 color{};
    };

    struct GraphValueRange
    {
        float minValue{ 0.f };
        float maxValue{ 1.f };
    };

    struct GraphDragState
    {
        bool active{ false };
        CurveTargetIdentity target{};
        size_t keyIndex{};
        ImVec2 startMousePos{};
        float startTime{};
        float startValue{};
        GraphValueRange startRange{};
    };

private: //## Data::CurveEditor
    vector<PinnedTrack> _pinnedTracks{};
    optional<CurveTargetIdentity> _activeTrack{};
    optional<size_t> _selectedKeyIndex{};
    bool _hasPendingCurveEdit{ false };
    Emitter_View::AuthoringSnapshot _pendingCurveBeforeSnapshot{};
    CurveTargetIdentity _pendingCurveTarget{};
    string _pendingCurveDescription{};
    GraphDragState _graphDrag{};
    float _trackListWidth{ 230.f };

private: //## Helper::CurveTarget
    static bool Is_SameIdentity(const CurveTargetIdentity& lhs, const CurveTargetIdentity& rhs);
    static const char* Get_ChannelLabel(CurveTargetChannel channel);
    static float Read_Vector2Channel(const Vec2& value, CurveTargetChannel channel);
    static void Write_Vector2Channel(Vec2& value, CurveTargetChannel channel, float channelValue);
    static float Read_Vector3Channel(const Vec3& value, CurveTargetChannel channel);
    static void Write_Vector3Channel(Vec3& value, CurveTargetChannel channel, float channelValue);
    static float Read_ColorChannel(const Color& value, CurveTargetChannel channel);
    static void Write_ColorChannel(Color& value, CurveTargetChannel channel, float channelValue);
    static string Build_Label(const AuthoringEmitter& emitter, const AuthoringModule& module, const string& propertyLabel, CurveTargetChannel channel);
    static ImU32 Get_TargetColor(const CurveTarget& target);
    static float Get_TargetValue(const CurveTarget& target, const FloatCurveKeyData& key);
    static float Get_TargetValue(const CurveTarget& target, const Vector2CurveKeyData& key);
    static float Get_TargetValue(const CurveTarget& target, const Vector3CurveKeyData& key);
    static float Get_TargetValue(const CurveTarget& target, const ColorCurveKeyData& key);
    static void Set_TargetValue(const CurveTarget& target, FloatCurveKeyData& key, float value);
    static void Set_TargetValue(const CurveTarget& target, Vector2CurveKeyData& key, float value);
    static void Set_TargetValue(const CurveTarget& target, Vector3CurveKeyData& key, float value);
    static void Set_TargetValue(const CurveTarget& target, ColorCurveKeyData& key, float value);
    static float Get_DefaultValue(const FloatDistributionData& distribution);
    static Vec2 Get_DefaultValue(const Vector2DistributionData& distribution);
    static Vec3 Get_DefaultValue(const Vector3DistributionData& distribution);
    static Color Get_DefaultValue(const ColorDistributionData& distribution);
    static ConstantCurveFloatDistributionData* Resolve_CurvePayload(FloatDistributionData& distribution);
    static ConstantCurveVector2DistributionData* Resolve_CurvePayload(Vector2DistributionData& distribution);
    static ConstantCurveVector3DistributionData* Resolve_CurvePayload(Vector3DistributionData& distribution);
    static ConstantCurveColorDistributionData* Resolve_CurvePayload(ColorDistributionData& distribution);
    static void Normalize_CurveKeys(ConstantCurveFloatDistributionData& curve);
    static void Normalize_CurveKeys(ConstantCurveVector2DistributionData& curve);
    static void Normalize_CurveKeys(ConstantCurveVector3DistributionData& curve);
    static void Normalize_CurveKeys(ConstantCurveColorDistributionData& curve);
    static float Evaluate_Curve(const ConstantCurveFloatDistributionData& curve, float x);
    static Vec2 Evaluate_Curve(const ConstantCurveVector2DistributionData& curve, float x);
    static Vec3 Evaluate_Curve(const ConstantCurveVector3DistributionData& curve, float x);
    static Color Evaluate_Curve(const ConstantCurveColorDistributionData& curve, float x);

    struct TargetEditRange
    {
        float minValue{ -1000.f };
        float maxValue{ 1000.f };
    };

    static TargetEditRange Compute_TargetEditRange(const CurveTarget& target);
    static float Clamp_GraphEditedValue(const CurveTarget& target, float value);

private: //## Helper::TargetResolve
    Shared<Emitter_View> Resolve_EmitterView() const;
    static vector<CurveTarget> Collect_ModuleTargets(AuthoringEmitter& emitter, AuthoringModule& module, bool requireActiveCurve = true);
    vector<CurveTarget> Resolve_PinnedTargets();
    optional<CurveTarget> Resolve_Target(const CurveTargetIdentity& identity) const;
    optional<size_t> Find_PinnedTrackIndex(const CurveTargetIdentity& identity) const;
    optional<size_t> Find_ResolvedTargetIndex(const vector<CurveTarget>& targets, const CurveTargetIdentity& identity) const;
    optional<CurveTarget> Get_ActiveTarget(const vector<CurveTarget>& targets) const;

    static void Add_FloatTarget(
        vector<CurveTarget>& targets,
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        FloatDistributionData& distribution,
        const string& propertyId,
        const string& label,
        ImU32 color,
        bool requireActiveCurve = true);
    static void Add_Vector2Target(
        vector<CurveTarget>& targets,
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        Vector2DistributionData& distribution,
        const string& propertyId,
        const string& label,
        bool requireActiveCurve = true);
    static void Add_Vector3Target(
        vector<CurveTarget>& targets,
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        Vector3DistributionData& distribution,
        const string& propertyId,
        const string& label,
        bool allowCurveTarget = true,
        bool requireActiveCurve = true);
    static void Add_ColorTarget(
        vector<CurveTarget>& targets,
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        ColorDistributionData& distribution,
        const string& propertyId,
        const string& label,
        bool requireActiveCurve = true);
    void Pin_Targets(const vector<CurveTarget>& targets);
    void Remove_StalePinnedTracks(const vector<CurveTarget>& targets);
    void Ensure_ActiveTrack(const vector<CurveTarget>& targets);

private: //## Helper::AuthoringHistory
    void Begin_CurveAuthoringEdit(const CurveTarget& target, const string& description);
    void Commit_CurveAuthoringEdit();
    bool Execute_CurveAuthoringEdit(const CurveTarget& target, const string& description, const function<bool()>& edit);
    void Mark_CurveChanged(const CurveTarget& target);

private: //## Helper::Render
    bool Convert_ActiveTargetToConstantCurve(const CurveTarget& target);
    GraphValueRange Compute_GraphValueRange(const vector<CurveTarget>& targets);
    bool Is_GraphDragActiveFor(const CurveTarget& target, size_t keyIndex) const;
    void Begin_GraphKeyDrag(
        const CurveTarget& target,
        size_t keyIndex,
        const ImVec2& mousePos,
        float startTime,
        float startValue,
        const GraphValueRange& valueRange);
    void End_GraphKeyDrag();
    void Draw_EmptyState();
    void Draw_TrackList(const vector<CurveTarget>& targets);
    void Draw_TargetHeader(CurveTarget& target);
    void Draw_CurveGraph(const vector<CurveTarget>& targets, CurveTarget& activeTarget);
    void Draw_SelectedKeyInspector(const CurveTarget& target);

public:
    static Shared<CurveEditor_View> Create();
};

NS_END
