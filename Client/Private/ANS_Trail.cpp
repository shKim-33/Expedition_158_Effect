#include "ANS_Trail.h"

#include "ContainerObject.h"
#include "EffectCom.h"
#include "Mon_Simon_Weapon.h"
#include "PartObject.h"

#include <atomic>

namespace
{
    constexpr const char* kTipExtendCurveJsonKey = "_tipExtendCurve";
    constexpr const char* kBaseLocalOffsetCurveJsonKey = "_baseLocalOffsetCurve";
    constexpr const char* kTipLocalOffsetCurveJsonKey = "_tipLocalOffsetCurve";
    constexpr const char* kTrailAnchorSourceJsonKey = "_trailAnchorSource";
    constexpr const char* kBaseBoneNameJsonKey = "_baseBoneName";
    constexpr const char* kBaseLocalOffsetJsonKey = "_baseLocalOffset";
    constexpr const char* kTipBoneNameJsonKey = "_tipBoneName";
    constexpr const char* kTipLocalOffsetJsonKey = "_tipLocalOffset";
    constexpr const char* kTipExtendEnabledJsonKey = "enabled";
    constexpr const char* kTipExtendKeysJsonKey = "keys";
    constexpr const char* kTipExtendPhaseJsonKey = "phase";
    constexpr const char* kTipExtendValueJsonKey = "value";
    constexpr const char* kTipExtendInterpolationJsonKey = "interpolation";
    constexpr float kDuplicatePhaseEpsilon = 0.0001f;

    float Clamp_TipExtendPhase(float phase)
    {
        return std::clamp(phase, 0.f, 1.f);
    }

    json Vec3_ToJson(const Vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    bool Json_ToVec3(const json& value, Vec3& outVec3)
    {
        if (!value.is_array() || value.size() < 3)
            return false;

        outVec3.x = value[0].get<float>();
        outVec3.y = value[1].get<float>();
        outVec3.z = value[2].get<float>();
        return true;
    }

    Mon_Simon_Weapon* Resolve_SimonWeapon(const AnimNotifyContext& context, const wstring& partTag)
    {
        if (context.owner == nullptr)
            return nullptr;

        Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
        while (current)
        {
            ContainerObject* ownerContainer = dynamic_cast<ContainerObject*>(current.get());
            if (ownerContainer != nullptr)
            {
                Mon_Simon_Weapon* weapon = dynamic_cast<Mon_Simon_Weapon*>(ownerContainer->Get_PartObject(partTag));
                if (weapon != nullptr)
                    return weapon;
            }

            current = current->Get_Owner();
        }

        return nullptr;
    }

    PartObject* Resolve_PartObject(const AnimNotifyContext& context, const wstring& partTag)
    {
        if (context.owner == nullptr)
            return nullptr;

        Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
        while (current)
        {
            ContainerObject* ownerContainer = dynamic_cast<ContainerObject*>(current.get());
            if (ownerContainer != nullptr)
            {
                if (PartObject* partObject = ownerContainer->Get_PartObject(partTag))
                    return partObject;
            }

            current = current->Get_Owner();
        }

        return nullptr;
    }
}

IMPLEMENT_REFLECTION(ANS_Trail)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Effect 이름 Override", _effectName);
    PROPERTY_WSTRING("대상 파츠", _partTag);

    return true;
}

void ANS_Trail::On_Begin(const AnimNotifyContext& context)
{
    Shared<EffectCom> effectCom = Resolve_EffectCom(context);
    if (effectCom == nullptr)
        return;

    PartObject* trailPart = Resolve_PartObject(context, _partTag);
    if (trailPart == nullptr)
        return;

    if (!trailPart->Is_Visible())
        return;

    _trailNotifyToken = Generate_TrailNotifyToken();
    _trailNotifyAnimationName = context.animationName;
    _trailNotifyStartSec = max(
        0.f,
        context.currentTimeSec - std::clamp(context.notifyStateProgress, 0.f, 1.f) * context.notifyStateDurationSec);
    _trailNotifyEndSec = _trailNotifyStartSec + max(0.f, context.notifyStateDurationSec);
    _trailNotifyLastTimeSec = context.currentTimeSec;

    EffectCom::TrailOffsetRuntimeDesc offsetDesc{};
    offsetDesc.phase = std::clamp(context.notifyStateProgress, 0.f, 1.f);
    offsetDesc.tipExtendCurve = _tipExtendCurve;
    offsetDesc.baseLocalOffsetCurve = _baseLocalOffsetCurve;
    offsetDesc.tipLocalOffsetCurve = _tipLocalOffsetCurve;

    EffectCom::TrailAnchorRuntimeDesc anchorDesc{};
    anchorDesc.source = _trailAnchorSource;
    anchorDesc.baseBoneName = _baseBoneName;
    anchorDesc.baseLocalOffset = _baseLocalOffset;
    anchorDesc.tipBoneName = _tipBoneName;
    anchorDesc.tipLocalOffset = _tipLocalOffset;

    effectCom->Play_TrailEffect(_effectName, _partTag, _layerTag, _trailNotifyToken, offsetDesc, anchorDesc);

    if (_trailAnchorSource == TrailAnchorSource::ModelDefault)
    {
        if (Mon_Simon_Weapon* simonWeapon = Resolve_SimonWeapon(context, _partTag))
        {
            Mon_Simon_Weapon::WeaponEchoTrailEventDesc trailEventDesc{};
            trailEventDesc.notifyToken = _trailNotifyToken;
            trailEventDesc.animationName = _trailNotifyAnimationName;
            trailEventDesc.startSec = _trailNotifyStartSec;
            trailEventDesc.durationSec = max(0.f, context.notifyStateDurationSec);
            trailEventDesc.sourceEffectName = _effectName.empty() ? simonWeapon->Get_TrailEffectName() : _effectName;
            trailEventDesc.layerTag = _layerTag;
            trailEventDesc.offsetDesc = offsetDesc;
            simonWeapon->Register_WeaponEchoTrailEvent(trailEventDesc);
        }
    }
}

void ANS_Trail::On_Tick(const AnimNotifyContext& context)
{
    if (_trailNotifyToken == 0)
        return;

    Shared<EffectCom> effectCom = Resolve_EffectCom(context);
    if (effectCom == nullptr)
        return;

    EffectCom::TrailOffsetRuntimeDesc offsetDesc{};
    offsetDesc.phase = std::clamp(context.notifyStateProgress, 0.f, 1.f);
    offsetDesc.tipExtendCurve = _tipExtendCurve;
    offsetDesc.baseLocalOffsetCurve = _baseLocalOffsetCurve;
    offsetDesc.tipLocalOffsetCurve = _tipLocalOffsetCurve;

    effectCom->Update_TrailEffectOffset(_effectName, _partTag, _trailNotifyToken, offsetDesc);
    _trailNotifyLastTimeSec = max(_trailNotifyStartSec, context.currentTimeSec);

    if (_trailAnchorSource == TrailAnchorSource::ModelDefault)
    {
        if (Mon_Simon_Weapon* simonWeapon = Resolve_SimonWeapon(context, _partTag))
            simonWeapon->Update_WeaponEchoTrailEvent(_trailNotifyToken, _trailNotifyLastTimeSec, offsetDesc);
    }
}

void ANS_Trail::On_End(const AnimNotifyContext& context)
{
    if (_trailNotifyToken == 0)
        return;

    float observedEndSec = _trailNotifyLastTimeSec;
    if (context.animationName == _trailNotifyAnimationName)
        observedEndSec = max(observedEndSec, context.currentTimeSec);

    const float sourceEndSec = _trailNotifyEndSec > _trailNotifyStartSec
        ? std::clamp(observedEndSec, _trailNotifyStartSec, _trailNotifyEndSec)
        : _trailNotifyStartSec;

    if (_trailAnchorSource == TrailAnchorSource::ModelDefault)
    {
        if (Mon_Simon_Weapon* simonWeapon = Resolve_SimonWeapon(context, _partTag))
            simonWeapon->End_WeaponEchoTrailEvent(_trailNotifyToken, sourceEndSec);
    }

    Shared<EffectCom> effectCom = Resolve_EffectCom(context);
    if (effectCom != nullptr)
        effectCom->Stop_TrailEffect(_effectName, _partTag, _trailNotifyToken);

    _trailNotifyToken = 0;
    _trailNotifyAnimationName.clear();
    _trailNotifyStartSec = 0.f;
    _trailNotifyEndSec = 0.f;
    _trailNotifyLastTimeSec = 0.f;
}

json ANS_Trail::To_Json() const
{
    json root = AnimNotifyState::To_Json();
    if (_trailAnchorSource == TrailAnchorSource::NotifyBonePair)
    {
        root[kTrailAnchorSourceJsonKey] = Get_TrailAnchorSourceName(_trailAnchorSource);
        root[kBaseBoneNameJsonKey] = _baseBoneName;
        root[kBaseLocalOffsetJsonKey] = Vec3_ToJson(_baseLocalOffset);
        root[kTipBoneNameJsonKey] = _tipBoneName;
        root[kTipLocalOffsetJsonKey] = Vec3_ToJson(_tipLocalOffset);
    }
    if (_tipExtendCurve.enabled || !_tipExtendCurve.keys.empty())
        root[kTipExtendCurveJsonKey] = TipExtendCurve_ToJson(_tipExtendCurve);
    if (_baseLocalOffsetCurve.enabled || !_baseLocalOffsetCurve.keys.empty())
        root[kBaseLocalOffsetCurveJsonKey] = LocalOffsetCurve_ToJson(_baseLocalOffsetCurve);
    if (_tipLocalOffsetCurve.enabled || !_tipLocalOffsetCurve.keys.empty())
        root[kTipLocalOffsetCurveJsonKey] = LocalOffsetCurve_ToJson(_tipLocalOffsetCurve);

    return root;
}

void ANS_Trail::From_Json(const json& data)
{
    AnimNotifyState::From_Json(data);

    _trailAnchorSource = TrailAnchorSource::ModelDefault;
    if (data.contains(kTrailAnchorSourceJsonKey))
        _trailAnchorSource = Parse_TrailAnchorSource(data[kTrailAnchorSourceJsonKey]);

    Clear_NotifyBonePair();
    if (_trailAnchorSource == TrailAnchorSource::NotifyBonePair)
    {
        _baseBoneName = data.value(kBaseBoneNameJsonKey, string{});
        if (data.contains(kBaseLocalOffsetJsonKey))
            (void)Json_ToVec3(data[kBaseLocalOffsetJsonKey], _baseLocalOffset);
        _tipBoneName = data.value(kTipBoneNameJsonKey, string{});
        if (data.contains(kTipLocalOffsetJsonKey))
            (void)Json_ToVec3(data[kTipLocalOffsetJsonKey], _tipLocalOffset);
    }

    _tipExtendCurve = {};
    if (data.contains(kTipExtendCurveJsonKey))
        _tipExtendCurve = TipExtendCurve_FromJson(data[kTipExtendCurveJsonKey]);

    _baseLocalOffsetCurve = {};
    if (data.contains(kBaseLocalOffsetCurveJsonKey))
        _baseLocalOffsetCurve = LocalOffsetCurve_FromJson(data[kBaseLocalOffsetCurveJsonKey]);

    _tipLocalOffsetCurve = {};
    if (data.contains(kTipLocalOffsetCurveJsonKey))
        _tipLocalOffsetCurve = LocalOffsetCurve_FromJson(data[kTipLocalOffsetCurveJsonKey]);

    Normalize_TipExtendCurve(_tipExtendCurve);
    Normalize_LocalOffsetCurve(_baseLocalOffsetCurve);
    Normalize_LocalOffsetCurve(_tipLocalOffsetCurve);
}

void ANS_Trail::Set_TipExtendCurve(const TipExtendCurve& curve)
{
    _tipExtendCurve = curve;
    Normalize_TipExtendCurve(_tipExtendCurve);
}

void ANS_Trail::Set_BaseLocalOffsetCurve(const LocalOffsetCurve& curve)
{
    _baseLocalOffsetCurve = curve;
    Normalize_LocalOffsetCurve(_baseLocalOffsetCurve);
}

void ANS_Trail::Set_TipLocalOffsetCurve(const LocalOffsetCurve& curve)
{
    _tipLocalOffsetCurve = curve;
    Normalize_LocalOffsetCurve(_tipLocalOffsetCurve);
}

void ANS_Trail::Clear_NotifyBonePair()
{
    _baseBoneName.clear();
    _baseLocalOffset = Vec3::Zero;
    _tipBoneName.clear();
    _tipLocalOffset = Vec3::Zero;
}

const char* ANS_Trail::Get_TrailAnchorSourceName(TrailAnchorSource source)
{
    switch (source)
    {
    case TrailAnchorSource::ModelDefault:
        return "ModelDefault";

    case TrailAnchorSource::NotifyBonePair:
        return "NotifyBonePair";
    }

    return "ModelDefault";
}

const char* ANS_Trail::Get_TipExtendInterpolationName(TipExtendInterpolation interpolation)
{
    switch (interpolation)
    {
    case TipExtendInterpolation::Linear:
        return "Linear";

    case TipExtendInterpolation::Constant:
        return "Constant";
    }

    return "Linear";
}

float ANS_Trail::Evaluate_TipExtendCurve(const TipExtendCurve& curve, float phase)
{
    if (curve.keys.empty())
        return 0.f;

    phase = std::clamp(phase, 0.f, 1.f);
    if (phase <= curve.keys.front().phase)
        return curve.keys.front().value;

    for (size_t keyIndex = 1; keyIndex < curve.keys.size(); ++keyIndex)
    {
        const TipExtendKey& previous = curve.keys[keyIndex - 1];
        const TipExtendKey& next = curve.keys[keyIndex];
        if (phase > next.phase)
            continue;

        if (previous.interpolation == TipExtendInterpolation::Constant)
            return previous.value;

        const float range = max(kDuplicatePhaseEpsilon, next.phase - previous.phase);
        const float t = std::clamp((phase - previous.phase) / range, 0.f, 1.f);
        return std::lerp(previous.value, next.value, t);
    }

    return curve.keys.back().value;
}

Vec3 ANS_Trail::Evaluate_LocalOffsetCurve(const LocalOffsetCurve& curve, float phase)
{
    if (curve.keys.empty())
        return Vec3::Zero;

    phase = std::clamp(phase, 0.f, 1.f);
    if (phase <= curve.keys.front().phase)
        return curve.keys.front().value;

    for (size_t keyIndex = 1; keyIndex < curve.keys.size(); ++keyIndex)
    {
        const LocalOffsetKey& previous = curve.keys[keyIndex - 1];
        const LocalOffsetKey& next = curve.keys[keyIndex];
        if (phase > next.phase)
            continue;

        if (previous.interpolation == TipExtendInterpolation::Constant)
            return previous.value;

        const float range = max(kDuplicatePhaseEpsilon, next.phase - previous.phase);
        const float t = std::clamp((phase - previous.phase) / range, 0.f, 1.f);
        return Vec3::Lerp(previous.value, next.value, t);
    }

    return curve.keys.back().value;
}

TrailAnchorAsset ANS_Trail::Make_OffsetTrailAnchorAsset(
    const TrailAnchorAsset& source,
    const TipExtendCurve& tipExtendCurve,
    const LocalOffsetCurve& baseLocalOffsetCurve,
    const LocalOffsetCurve& tipLocalOffsetCurve,
    float phase)
{
    TrailAnchorAsset result = source;
    result.base.localPosition += Evaluate_LocalOffsetCurve(baseLocalOffsetCurve, phase);
    result.tip.localPosition += Evaluate_LocalOffsetCurve(tipLocalOffsetCurve, phase);

    if (Has_TipExtendCurvePayload(tipExtendCurve))
    {
        Vec3 tipDirection = source.tip.localPosition - source.base.localPosition;
        if (tipDirection.LengthSquared() > 0.000001f)
        {
            tipDirection.Normalize();
            result.tip.localPosition += tipDirection * Evaluate_TipExtendCurve(tipExtendCurve, phase);
        }
    }

    return result;
}

ANS_Trail::TrailAnchorSource ANS_Trail::Parse_TrailAnchorSource(const json& data)
{
    if (data.is_string())
    {
        const string source = data.get<string>();
        if (source == "NotifyBonePair")
            return TrailAnchorSource::NotifyBonePair;

        return TrailAnchorSource::ModelDefault;
    }

    if (data.is_number_integer())
    {
        const int32 source = data.get<int32>();
        if (source == static_cast<int32>(TrailAnchorSource::NotifyBonePair))
            return TrailAnchorSource::NotifyBonePair;
    }

    return TrailAnchorSource::ModelDefault;
}

ANS_Trail::TipExtendInterpolation ANS_Trail::Parse_TipExtendInterpolation(const json& data)
{
    if (data.is_string())
    {
        const string interpolation = data.get<string>();
        if (interpolation == "Constant")
            return TipExtendInterpolation::Constant;

        return TipExtendInterpolation::Linear;
    }

    if (data.is_number_integer())
    {
        const int32 interpolation = data.get<int32>();
        if (interpolation == static_cast<int32>(TipExtendInterpolation::Constant))
            return TipExtendInterpolation::Constant;
    }

    return TipExtendInterpolation::Linear;
}

bool ANS_Trail::Has_TipExtendCurvePayload(const TipExtendCurve& curve)
{
    return curve.enabled || !curve.keys.empty();
}

bool ANS_Trail::Has_LocalOffsetCurvePayload(const LocalOffsetCurve& curve)
{
    return curve.enabled || !curve.keys.empty();
}

json ANS_Trail::TipExtendCurve_ToJson(const TipExtendCurve& curve)
{
    TipExtendCurve normalized = curve;
    Normalize_TipExtendCurve(normalized);

    json root = json::object();
    root[kTipExtendEnabledJsonKey] = normalized.enabled;
    root[kTipExtendKeysJsonKey] = json::array();

    for (const TipExtendKey& key : normalized.keys)
    {
        json keyNode = json::object();
        keyNode[kTipExtendPhaseJsonKey] = key.phase;
        keyNode[kTipExtendValueJsonKey] = key.value;
        keyNode[kTipExtendInterpolationJsonKey] = Get_TipExtendInterpolationName(key.interpolation);
        root[kTipExtendKeysJsonKey].push_back(keyNode);
    }

    return root;
}

ANS_Trail::TipExtendCurve ANS_Trail::TipExtendCurve_FromJson(const json& data)
{
    TipExtendCurve curve{};
    if (!data.is_object())
        return curve;

    curve.enabled = data.value(kTipExtendEnabledJsonKey, false);

    const json keysNode = data.value(kTipExtendKeysJsonKey, json::array());
    if (keysNode.is_array())
    {
        for (const json& keyNode : keysNode)
        {
            if (!keyNode.is_object())
                continue;

            TipExtendKey key{};
            key.phase = keyNode.value(kTipExtendPhaseJsonKey, 0.f);
            key.value = keyNode.value(kTipExtendValueJsonKey, 0.f);

            if (keyNode.contains(kTipExtendInterpolationJsonKey))
                key.interpolation = Parse_TipExtendInterpolation(keyNode[kTipExtendInterpolationJsonKey]);

            curve.keys.push_back(key);
        }
    }

    Normalize_TipExtendCurve(curve);
    return curve;
}

json ANS_Trail::LocalOffsetCurve_ToJson(const LocalOffsetCurve& curve)
{
    LocalOffsetCurve normalized = curve;
    Normalize_LocalOffsetCurve(normalized);

    json root = json::object();
    root[kTipExtendEnabledJsonKey] = normalized.enabled;
    root[kTipExtendKeysJsonKey] = json::array();

    for (const LocalOffsetKey& key : normalized.keys)
    {
        json keyNode = json::object();
        keyNode[kTipExtendPhaseJsonKey] = key.phase;
        keyNode[kTipExtendValueJsonKey] = Vec3_ToJson(key.value);
        keyNode[kTipExtendInterpolationJsonKey] = Get_TipExtendInterpolationName(key.interpolation);
        root[kTipExtendKeysJsonKey].push_back(keyNode);
    }

    return root;
}

ANS_Trail::LocalOffsetCurve ANS_Trail::LocalOffsetCurve_FromJson(const json& data)
{
    LocalOffsetCurve curve{};
    if (!data.is_object())
        return curve;

    curve.enabled = data.value(kTipExtendEnabledJsonKey, false);

    const json keysNode = data.value(kTipExtendKeysJsonKey, json::array());
    if (keysNode.is_array())
    {
        for (const json& keyNode : keysNode)
        {
            if (!keyNode.is_object())
                continue;

            LocalOffsetKey key{};
            key.phase = keyNode.value(kTipExtendPhaseJsonKey, 0.f);

            if (keyNode.contains(kTipExtendValueJsonKey))
                (void)Json_ToVec3(keyNode[kTipExtendValueJsonKey], key.value);

            if (keyNode.contains(kTipExtendInterpolationJsonKey))
                key.interpolation = Parse_TipExtendInterpolation(keyNode[kTipExtendInterpolationJsonKey]);

            curve.keys.push_back(key);
        }
    }

    Normalize_LocalOffsetCurve(curve);
    return curve;
}

void ANS_Trail::Normalize_TipExtendCurve(TipExtendCurve& curve)
{
    vector<TipExtendKey> normalizedKeys{};
    normalizedKeys.reserve(curve.keys.size());

    for (TipExtendKey key : curve.keys)
    {
        key.phase = Clamp_TipExtendPhase(key.phase);

        auto duplicateIter = ranges::find_if(
            normalizedKeys,
            [phase = key.phase](const TipExtendKey& existing)
            {
                return std::abs(existing.phase - phase) <= kDuplicatePhaseEpsilon;
            });

        if (duplicateIter != normalizedKeys.end())
            *duplicateIter = key;
        else
            normalizedKeys.push_back(key);
    }

    ranges::sort(
        normalizedKeys,
        [](const TipExtendKey& lhs, const TipExtendKey& rhs)
        {
            return lhs.phase < rhs.phase;
        });

    curve.keys = std::move(normalizedKeys);
}

void ANS_Trail::Normalize_LocalOffsetCurve(LocalOffsetCurve& curve)
{
    vector<LocalOffsetKey> normalizedKeys{};
    normalizedKeys.reserve(curve.keys.size());

    for (LocalOffsetKey key : curve.keys)
    {
        key.phase = Clamp_TipExtendPhase(key.phase);

        auto duplicateIter = ranges::find_if(
            normalizedKeys,
            [phase = key.phase](const LocalOffsetKey& existing)
            {
                return std::abs(existing.phase - phase) <= kDuplicatePhaseEpsilon;
            });

        if (duplicateIter != normalizedKeys.end())
            *duplicateIter = key;
        else
            normalizedKeys.push_back(key);
    }

    ranges::sort(
        normalizedKeys,
        [](const LocalOffsetKey& lhs, const LocalOffsetKey& rhs)
        {
            return lhs.phase < rhs.phase;
        });

    curve.keys = std::move(normalizedKeys);
}

uint64 ANS_Trail::Generate_TrailNotifyToken()
{
    static std::atomic_uint64_t nextToken = 1;
    return nextToken.fetch_add(1, std::memory_order_relaxed);
}

Shared<EffectCom> ANS_Trail::Resolve_EffectCom(const AnimNotifyContext& context) const
{
    // Trail partTag는 container owner 기준이므로 가장 가까운 part가 아니라 owner root의 EffectCom을 사용한다.
    return EffectCom::Find_FromNotifyOwner(context.owner, EffectCom::EffectPlayTarget::PartRoot);
}
