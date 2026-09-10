#pragma once

#include "ContainerObject.h"
#include "EffectRuntime_Types.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewMonsterBodyObject;
class TrailPreviewMonsterHairObject;
class TrailPreviewMonsterWeaponObject;

class TrailPreviewMonsterObject final
    : public ContainerObject
      , public IEffectTrailSampleProvider
      , public IEffectSourcePointSampleProvider
{
public:
    TrailPreviewMonsterObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TrailPreviewMonsterObject(const TrailPreviewMonsterObject& prototype);
    ~TrailPreviewMonsterObject() override;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;

public: //## Accessors::Preview
    Shared<ModelCom> Get_BodyModel() const;
    void Set_PreviewVisible(bool visible);

public: //## Behavior::SampleProvider
    bool Has_TrailSample() const;
    bool Try_GetTrailSample(EffectTrailSample& outSample) const override;
    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override;

private: //## Static::PreviewFixture
    inline static const Vec3 kFallbackBaseLocalOffset{ 0.f, 0.f, 0.f };
    inline static const Vec3 kFallbackTipLocalOffset{ 0.f, 0.f, 4.f };
    inline static const Vec3 kFallbackSourceLocalOffset{ 0.f, 0.f, 4.f };
    inline static const float kTrailAnchorMarkerRadius{ 0.03f };

private: //## Data::Parts
    Shared<TrailPreviewMonsterBodyObject> _body{};
    Shared<TrailPreviewMonsterHairObject> _hair{};
    Shared<TrailPreviewMonsterWeaponObject> _weapon{};

private: //## Data::TrailSample
    Vec3 _trailBaseWorldPosition{};
    Vec3 _trailTipWorldPosition{};
    Vec3 _sourcePointWorldPosition{};
    Quat _trailBaseWorldRotation{ Quat::Identity };
    Quat _trailTipWorldRotation{ Quat::Identity };
    Quat _sourcePointWorldRotation{ Quat::Identity };
    bool _hasTrailSample{ false };
    Client::TrailAnchorAsset _trailAnchorAsset{};
    bool _hasTrailAnchorAsset{ false };
    bool _anchorFallbackWarningLogged{ false };

private: //## Helper::Setup
    HRESULT Ready_PreviewParts();
    HRESULT Restore_VisualPartsFromPrefab();
    HRESULT Restore_VisualPartFromPrefab(const json& partNode);
    void Load_TrailAnchorAsset(const string& modelGuid);

private: //## Helper::TrailSample
    void Clear_TrailSample();
    void Update_TrailPoints();
    bool Try_ResolveWeaponLocalPoint(const Vec3& localPosition, Vec3& outWorldPosition, Quat& outWorldRotation) const;
    bool Try_ResolveTrailAnchorPoint(const Client::TrailAnchorPointDesc& anchor, const Vec3& localOffset, Vec3& outWorldPosition, Quat& outWorldRotation) const;
    bool Try_ResolveSourceAnchorPoint(const Vec3& localOffset, Vec3& outWorldPosition, Quat& outWorldRotation) const;
    bool Try_ResolveFallbackLocalPoint(const Vec3& localPosition, const Vec3& localOffset, Vec3& outWorldPosition, Quat& outWorldRotation) const;

private: //## Helper::Render
    void Draw_TrailAnchorMarkers() const;
    void Draw_TrailAnchorMarker(const Vec3& worldPosition, const Color& color) const;

public:
    static Shared<TrailPreviewMonsterObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
