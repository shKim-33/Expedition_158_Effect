#include "TrailPreviewController.h"

#include "EffectEditorInstance.h"
#include "ModelCom.h"
#include "PlayableParts.h"
#include "TrailPreviewMonsterObject.h"
#include "TrailPreviewPlayerObject.h"

NS_BEGIN(EffectEditor)

TrailPreviewController::TrailPreviewController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

TrailPreviewController::TrailPreviewController(const TrailPreviewController& prototype)
    : GameObject{ prototype }
{
}

HRESULT TrailPreviewController::Initialize(void* arg)
{
    const PreviewDesc* previewDesc = static_cast<PreviewDesc*>(arg);
    CHECK_NULL(previewDesc, E_FAIL);
    CHECK_NULL(previewDesc->player, E_FAIL);
    CHECK_NULL(previewDesc->monster, E_FAIL);

    _player = previewDesc->player;
    _monster = previewDesc->monster;

    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    Set_Name(L"TrailPreview_Controller");
    Set_Visible(false);
    Set_PlayerVisible(false);
    Set_MonsterVisible(false);
    Reset_PreviewPose();

    return S_OK;
}

void TrailPreviewController::Priority_Update(float)
{
    const bool previewVisible =
        nullptr != EDITOR &&
        EDITOR->Is_TrailPreviewVisible();
    const TrailPreviewCharacterSlot slot = previewVisible
                                           ? EDITOR->Get_TrailPreviewCharacterSlot()
                                           : TrailPreviewCharacterSlot::Player;
    const bool playerVisible = previewVisible && slot == TrailPreviewCharacterSlot::Player;
    const bool monsterVisible = previewVisible && slot == TrailPreviewCharacterSlot::Monster;

    Set_Visible(previewVisible);
    Set_PlayerVisible(playerVisible);
    Set_MonsterVisible(monsterVisible);

    if (!previewVisible)
    {
        Reset_PreviewPose();
        return;
    }

    Ensure_CurrentMotion();
    Apply_AnimationSpeed(Get_BodyModel());
}

void TrailPreviewController::Update(float)
{
}

bool TrailPreviewController::Try_GetCurrentMotionRatio(float& outRatio) const
{
    outRatio = 0.f;

    if (!_animationInitialized)
        return false;

    if (_currentMotion != AppliedMotion::AutoAttack &&
        _currentMotion != AppliedMotion::Manual)
        return false;

    const Shared<ModelCom> bodyModel = Get_BodyModel();
    if (bodyModel == nullptr)
        return false;

    if (_currentMotion == AppliedMotion::AutoAttack)
    {
        const char* currentAnimationName = bodyModel->Get_AnimationName(bodyModel->Get_CurrentAnimationIndex());
        const string attackAnimationName = Resolve_AutoAnimationName(bodyModel, _currentSlot, true);
        if (currentAnimationName == nullptr || attackAnimationName.empty() || strcmp(currentAnimationName, attackAnimationName.c_str()) != 0)
            return false;
    }

    return Try_GetCurrentAnimationRatio(bodyModel, outRatio);
}

void TrailPreviewController::Get_AnimationNames(vector<string>& outNames) const
{
    outNames.clear();

    const Shared<ModelCom> bodyModel = Get_BodyModel();
    if (nullptr == bodyModel)
        return;

    const uint32 numAnimations = static_cast<uint32>(bodyModel->Get_NumAnimations());
    outNames.reserve(numAnimations);
    for (uint32 animationIndex = 0; animationIndex < numAnimations; ++animationIndex)
    {
        const char* animationName = bodyModel->Get_AnimationName(animationIndex);
        if (nullptr != animationName && animationName[0] != '\0')
            outNames.emplace_back(animationName);
    }
}

void TrailPreviewController::Set_PlayerVisible(bool visible)
{
    const Shared<TrailPreviewPlayerObject> player = _player.lock();
    if (nullptr != player)
        player->Set_PreviewVisible(visible);
}

void TrailPreviewController::Set_MonsterVisible(bool visible)
{
    const Shared<TrailPreviewMonsterObject> monster = _monster.lock();
    if (nullptr != monster)
        monster->Set_PreviewVisible(visible);
}

void TrailPreviewController::Reset_PreviewPose()
{
    _animationInitialized = false;
    _currentMotion = AppliedMotion::None;
    _currentSlot = TrailPreviewCharacterSlot::Player;
    _currentManualAnimationName.clear();

    Ensure_CurrentMotion();
}

Shared<ModelCom> TrailPreviewController::Get_BodyModel() const
{
    if (EDITOR != nullptr && EDITOR->Get_TrailPreviewCharacterSlot() == TrailPreviewCharacterSlot::Monster)
        return Get_MonsterBodyModel();

    return Get_PlayerBodyModel();
}

Shared<ModelCom> TrailPreviewController::Get_PlayerBodyModel() const
{
    const Shared<TrailPreviewPlayerObject> player = _player.lock();
    if (nullptr == player)
        return nullptr;

    const PlayableParts* bodyPart =
        dynamic_cast<PlayableParts*>(player->Get_PartObject(L"Part_Body"));
    if (nullptr == bodyPart)
        return nullptr;

    return bodyPart->Get_Model();
}

Shared<ModelCom> TrailPreviewController::Get_MonsterBodyModel() const
{
    const Shared<TrailPreviewMonsterObject> monster = _monster.lock();
    if (nullptr == monster)
        return nullptr;

    return monster->Get_BodyModel();
}

void TrailPreviewController::Ensure_CurrentMotion()
{
    const Shared<ModelCom> bodyModel = Get_BodyModel();
    if (nullptr == bodyModel)
        return;

    if (nullptr == EDITOR)
        return;

    switch (EDITOR->Get_TrailPreviewMotion())
    {
    case TrailPreviewMotion::Manual:
        Ensure_Animation(
            bodyModel,
            AppliedMotion::Manual,
            EDITOR->Get_TrailPreviewManualAnimationName(),
            true
        );
        break;
    case TrailPreviewMotion::Auto:
    default:
        Ensure_AutoMotion(bodyModel);
        break;
    }
}

void TrailPreviewController::Ensure_AutoMotion(const Shared<ModelCom>& bodyModel)
{
    if (nullptr == EDITOR)
        return;

    if (EDITOR->IsPaused() && _animationInitialized)
        return;

    const bool playAttack = EDITOR->IsPlaying();
    const TrailPreviewCharacterSlot currentSlot = Get_CurrentSlot();
    const AppliedMotion motion = playAttack ? AppliedMotion::AutoAttack : AppliedMotion::AutoRest;
    const string animationName = Resolve_AutoAnimationName(bodyModel, currentSlot, playAttack);
    if (animationName.empty())
    {
        _animationInitialized = false;
        _currentMotion = AppliedMotion::None;
        _currentManualAnimationName.clear();
        return;
    }

    Ensure_Animation(bodyModel, motion, animationName, playAttack);
}

void TrailPreviewController::Ensure_Animation(
    const Shared<ModelCom>& bodyModel,
    AppliedMotion motion,
    const string& animationName,
    bool loop)
{
    if (nullptr == bodyModel)
        return;

    const TrailPreviewCharacterSlot currentSlot = Get_CurrentSlot();

    if (_animationInitialized &&
        _currentSlot == currentSlot &&
        _currentMotion == motion &&
        _currentManualAnimationName == animationName)
        return;

    if (animationName.empty())
    {
        _animationInitialized = false;
        _currentMotion = AppliedMotion::None;
        _currentManualAnimationName.clear();
        return;
    }

    if (!Has_Animation(bodyModel, animationName.c_str()))
    {
        LOG_WARN_ONCE("[TrailPreview] animation '{}' not found.", animationName);
        _animationInitialized = true;
        _currentSlot = currentSlot;
        _currentMotion = motion;
        _currentManualAnimationName = animationName;
        return;
    }

    bodyModel->Set_Animation(animationName, loop, 0.f);
    bodyModel->Set_AnimationPaused(false);
    Apply_AnimationSpeed(bodyModel);
    bodyModel->Set_CurrentAnimationTimeSec(0.f);
    bodyModel->Refresh_CurrentAnimationPose();

    _animationInitialized = true;
    _currentSlot = currentSlot;
    _currentMotion = motion;
    _currentManualAnimationName = animationName;
}

void TrailPreviewController::Apply_AnimationSpeed(const Shared<ModelCom>& bodyModel) const
{
    if (nullptr == bodyModel || nullptr == EDITOR)
        return;

    bodyModel->Set_FrameSpeed(EDITOR->Get_TrailPreviewAnimationSpeed());
}

bool TrailPreviewController::Has_Animation(const Shared<ModelCom>& model, const char* animationName) const
{
    if (nullptr == model || nullptr == animationName)
        return false;

    const uint32 numAnimations = static_cast<uint32>(model->Get_NumAnimations());
    for (uint32 animationIndex = 0; animationIndex < numAnimations; ++animationIndex)
    {
        const char* currentName = model->Get_AnimationName(animationIndex);
        if (nullptr != currentName && strcmp(currentName, animationName) == 0)
            return true;
    }

    return false;
}

TrailPreviewCharacterSlot TrailPreviewController::Get_CurrentSlot() const
{
    return EDITOR != nullptr
           ? EDITOR->Get_TrailPreviewCharacterSlot()
           : TrailPreviewCharacterSlot::Player;
}

string TrailPreviewController::Resolve_AutoAnimationName(
    const Shared<ModelCom>& bodyModel,
    TrailPreviewCharacterSlot slot,
    bool playAttack) const
{
    if (slot == TrailPreviewCharacterSlot::Player)
        return playAttack ? kAttackAnimationName : kRestAnimationName;

    if (playAttack)
    {
        return Resolve_FirstExistingAnimationName(
            bodyModel,
            {
                "Simon_Skill_3",
                "Simon_Skill_1",
                "Simon_Skill_2",
                "Simon_Skill04",
                "Simon_Skill6_Attack",
            });
    }

    return Resolve_FirstExistingAnimationName(
        bodyModel,
        {
            "Simon_Idle",
            "Simon_Battle_Idle",
            "Simon_Idle_Battle",
            "Simon_Stand",
        });
}

string TrailPreviewController::Resolve_FirstExistingAnimationName(
    const Shared<ModelCom>& bodyModel,
    const initializer_list<const char*>& candidates) const
{
    for (const char* candidate : candidates)
    {
        if (Has_Animation(bodyModel, candidate))
            return candidate;
    }

    return {};
}

bool TrailPreviewController::Try_GetCurrentAnimationRatio(const Shared<ModelCom>& bodyModel, float& outRatio) const
{
    outRatio = 0.f;

    if (nullptr == bodyModel)
        return false;

    const float durationSec = bodyModel->Get_CurrentAnimationDurationSec();
    if (durationSec <= 0.f)
        return false;

    outRatio = clamp(bodyModel->Get_CurrentAnimationTimeSec() / durationSec, 0.f, 1.f);
    return true;
}

Shared<TrailPreviewController> TrailPreviewController::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewController");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewController::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewController>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewController");
        MSG_BOX("Failed to Clone : TrailPreviewController");
        return nullptr;
    }

    return instance;
}

void TrailPreviewController::Free()
{
    __super::Free();
}

NS_END
