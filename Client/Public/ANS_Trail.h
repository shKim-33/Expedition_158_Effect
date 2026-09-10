#pragma once

#include "AnimNotifyState.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Client)

class EffectCom;

class ANS_Trail : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Trail)

public: //## Types::Public
    enum class TrailAnchorSource : uint8
    {
        ModelDefault,
        NotifyBonePair
    };

    enum class TipExtendInterpolation : uint8
    {
        Linear,
        Constant
    };

    struct TipExtendKey
    {
        float phase = 0.f;
        float value = 0.f;
        TipExtendInterpolation interpolation = TipExtendInterpolation::Linear;
    };

    struct TipExtendCurve
    {
        bool enabled = false;
        vector<TipExtendKey> keys{};
    };

    struct LocalOffsetKey
    {
        float phase = 0.f;
        Vec3 value = Vec3::Zero;
        TipExtendInterpolation interpolation = TipExtendInterpolation::Linear;
    };

    struct LocalOffsetCurve
    {
        bool enabled = false;
        vector<LocalOffsetKey> keys{};
    };

public:
    ANS_Trail() = default;
    ~ANS_Trail() override = default;

public:
    string Get_TypeName() const override { return "ANS_Trail"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;
    json To_Json() const override;
    void From_Json(const json& data) override;

public: //## Behavior::TrailConfiguration
    static const char* Get_TrailAnchorSourceName(TrailAnchorSource source);
    static const char* Get_TipExtendInterpolationName(TipExtendInterpolation interpolation);
    static float Evaluate_TipExtendCurve(const TipExtendCurve& curve, float phase);
    static Vec3 Evaluate_LocalOffsetCurve(const LocalOffsetCurve& curve, float phase);
    static TrailAnchorAsset Make_OffsetTrailAnchorAsset(
        const TrailAnchorAsset& source,
        const TipExtendCurve& tipExtendCurve,
        const LocalOffsetCurve& baseLocalOffsetCurve,
        const LocalOffsetCurve& tipLocalOffsetCurve,
        float phase);

    const TipExtendCurve& Get_TipExtendCurve() const { return _tipExtendCurve; }
    const LocalOffsetCurve& Get_BaseLocalOffsetCurve() const { return _baseLocalOffsetCurve; }
    const LocalOffsetCurve& Get_TipLocalOffsetCurve() const { return _tipLocalOffsetCurve; }
    TrailAnchorSource Get_TrailAnchorSource() const { return _trailAnchorSource; }
    const string& Get_BaseBoneName() const { return _baseBoneName; }
    const Vec3& Get_BaseLocalOffset() const { return _baseLocalOffset; }
    const string& Get_TipBoneName() const { return _tipBoneName; }
    const Vec3& Get_TipLocalOffset() const { return _tipLocalOffset; }

    void Set_TipExtendCurve(const TipExtendCurve& curve);
    void Set_BaseLocalOffsetCurve(const LocalOffsetCurve& curve);
    void Set_TipLocalOffsetCurve(const LocalOffsetCurve& curve);
    void Set_TrailAnchorSource(TrailAnchorSource source)
    {
        _trailAnchorSource = source;
        if (_trailAnchorSource == TrailAnchorSource::ModelDefault)
            Clear_NotifyBonePair();
    }
    void Set_BaseBoneName(const string& boneName) { _baseBoneName = boneName; }
    void Set_BaseLocalOffset(const Vec3& localOffset) { _baseLocalOffset = localOffset; }
    void Set_TipBoneName(const string& boneName) { _tipBoneName = boneName; }
    void Set_TipLocalOffset(const Vec3& localOffset) { _tipLocalOffset = localOffset; }
    void Clear_NotifyBonePair();

private: //## Data::TrailBinding
    wstring _effectName = L"";
    wstring _partTag = L"Part_Weapon";
    wstring _layerTag = L"Layer_Effect";
    TrailAnchorSource _trailAnchorSource = TrailAnchorSource::ModelDefault;
    string _baseBoneName{};
    Vec3 _baseLocalOffset{ Vec3::Zero };
    string _tipBoneName{};
    Vec3 _tipLocalOffset{ Vec3::Zero };
    TipExtendCurve _tipExtendCurve{};
    LocalOffsetCurve _baseLocalOffsetCurve{};
    LocalOffsetCurve _tipLocalOffsetCurve{};
    uint64 _trailNotifyToken = 0;
    string _trailNotifyAnimationName{};
    float _trailNotifyStartSec = 0.f;
    float _trailNotifyEndSec = 0.f;
    float _trailNotifyLastTimeSec = 0.f;

private: //## Helper::Serialization
    static TrailAnchorSource Parse_TrailAnchorSource(const json& data);
    static TipExtendInterpolation Parse_TipExtendInterpolation(const json& data);
    static bool Has_TipExtendCurvePayload(const TipExtendCurve& curve);
    static bool Has_LocalOffsetCurvePayload(const LocalOffsetCurve& curve);
    static json TipExtendCurve_ToJson(const TipExtendCurve& curve);
    static TipExtendCurve TipExtendCurve_FromJson(const json& data);
    static json LocalOffsetCurve_ToJson(const LocalOffsetCurve& curve);
    static LocalOffsetCurve LocalOffsetCurve_FromJson(const json& data);
    static void Normalize_TipExtendCurve(TipExtendCurve& curve);
    static void Normalize_LocalOffsetCurve(LocalOffsetCurve& curve);

private: //## Helper::Runtime
    static uint64 Generate_TrailNotifyToken();
    Shared<EffectCom> Resolve_EffectCom(const AnimNotifyContext& context) const;
};

NS_END
