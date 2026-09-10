#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)
class Body_Player;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewWeaponObject;

class TrailPreviewPlayerObject final : public Player_BattleBase
{
public:
    TrailPreviewPlayerObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TrailPreviewPlayerObject(const TrailPreviewPlayerObject& prototype);
    ~TrailPreviewPlayerObject() override;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;

public: //## Accessors::Preview
    Shared<TrailPreviewWeaponObject> Get_PreviewWeapon() const;
    void Set_PreviewVisible(bool visible);

    HeroId Get_HeroId() const override { return HeroId::Gustave; }

private: //## Data::PreviewWeapon
    bool _deferPreviewWeaponCreation{ true };

private: //## Helper::PrefabRestore
    HRESULT Restore_VisualPartsFromPrefab();
    HRESULT Restore_VisualPartFromPrefab(const json& partNode);

private: //## Hook::PlayerParts
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;
    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }
    void Get_WeaponAttachDescs(vector<WeaponAttachDesc>& outDescs) const override;
    HRESULT Ready_WeaponParts(Body_Player* body) override;

public:
    static Shared<TrailPreviewPlayerObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
