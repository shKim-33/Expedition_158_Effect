#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewMonsterObject;
class TrailPreviewPlayerObject;
enum class TrailPreviewCharacterSlot;

class TrailPreviewController final : public GameObject
{
public:
    struct PreviewDesc : public GAMEOBJECT_DESC
    {
        Shared<TrailPreviewPlayerObject> player{};
        Shared<TrailPreviewMonsterObject> monster{};
    };

public:
    TrailPreviewController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TrailPreviewController(const TrailPreviewController& prototype);
    ~TrailPreviewController() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;

public:
    bool Try_GetCurrentMotionRatio(float& outRatio) const;
    void Get_AnimationNames(vector<string>& outNames) const;

private: //## Types::Animation
    enum class AppliedMotion
    {
        None,
        AutoRest,
        AutoAttack,
        Manual,
    };

private: //## Static::Animation
    static constexpr auto kAttackAnimationName{ "Gustave_Noah_Skill_Blitz" };
    static constexpr auto kRestAnimationName{ "Gustave_Anim_Noah_Idle_World_FeetFixed" };

private: //## Data::Animation
    Weak<TrailPreviewPlayerObject> _player{};
    Weak<TrailPreviewMonsterObject> _monster{};
    AppliedMotion _currentMotion{ AppliedMotion::None };
    TrailPreviewCharacterSlot _currentSlot{};
    string _currentManualAnimationName{};
    bool _animationInitialized{ false };

private: //## Helper::Animation
    void Set_PlayerVisible(bool visible);
    void Set_MonsterVisible(bool visible);
    void Reset_PreviewPose();
    Shared<ModelCom> Get_BodyModel() const;
    Shared<ModelCom> Get_PlayerBodyModel() const;
    Shared<ModelCom> Get_MonsterBodyModel() const;
    void Ensure_CurrentMotion();
    void Ensure_AutoMotion(const Shared<ModelCom>& bodyModel);
    void Ensure_Animation(const Shared<ModelCom>& bodyModel, AppliedMotion motion, const string& animationName, bool loop);
    void Apply_AnimationSpeed(const Shared<ModelCom>& bodyModel) const;
    bool Has_Animation(const Shared<ModelCom>& model, const char* animationName) const;
    TrailPreviewCharacterSlot Get_CurrentSlot() const;
    string Resolve_AutoAnimationName(const Shared<ModelCom>& bodyModel, TrailPreviewCharacterSlot slot, bool playAttack) const;
    string Resolve_FirstExistingAnimationName(const Shared<ModelCom>& bodyModel, const initializer_list<const char*>& candidates) const;
    bool Try_GetCurrentAnimationRatio(const Shared<ModelCom>& bodyModel, float& outRatio) const;

public:
    static Shared<TrailPreviewController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
