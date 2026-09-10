#include "AN_SpawnLineGroundEffect.h"

#include "BattleController.h"
#include "Client_Constants.h"
#include "EffectCom.h"
#include "ClientInstance.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(AN_SpawnLineGroundEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("이펙트 이름", _effectName);
    PROPERTY_WSTRING("레이어 태그", _layerTag);
    PROPERTY_INT("몇등분", _segmentNum);
    PROPERTY_INT("지금 위치", _segmentIndex);
    PROPERTY_VEC3("로컬 오프셋", _localOffset);
    PROPERTY_VEC3("로컬 회전", _localRotation);
    PROPERTY_VEC3("스케일", _scale);
    PROPERTY_FLOAT ("Y", _yPos);

    return true;
}

void AN_SpawnLineGroundEffect::Execute(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner);
    if (effectCom == nullptr)
        return;


    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    auto& turnAction = battle->Get_TurnAction();

    const auto& steps = turnAction.GetSteps();
    const uint32 curStepIndex = turnAction.GetCurStepIndex();

    if (curStepIndex >= steps.size())
        return;

    const auto& targets = steps[curStepIndex].targets;
    if (targets.empty())
        return;

    for (int i = 0; i < targets.size(); i++)
    {
        Vec3 AttackerPos = turnAction.Get_CurChar().actor->Get_Transform()->Get_Position();
        Vec3 TargetPos = targets[i].actor->Get_Transform()->Get_Position();
        //AttackerPos.y = _yPos;
        //TargetPos.y = _yPos;
  
        TargetPos.y = AttackerPos.y;
        Vec3 dir = TargetPos - AttackerPos;
        float length = dir.Length();
        dir.Normalize();

       auto segment =  length / static_cast<float>((_segmentNum - 1));

       Vec3 spawnPos = AttackerPos + dir * (static_cast<float>(_segmentIndex) * segment);

        PlayEffectDesc desc{};
        desc.effectName = _effectName;
        desc.layerTag = _layerTag;
        desc.worldPosition = spawnPos;
        desc.localOffset = _localOffset;
        desc.localRotation = _localRotation;
        if (_segmentIndex <= 0)
            return;

        float scaleValue = 1.2f;
        if (_segmentIndex == 1)
            scaleValue = 0.6f;
        else if (_segmentIndex == 2)
            scaleValue = 0.9f;
        else if (_segmentIndex == 3)
            scaleValue = 1.2f;
        desc.scale = Vec3(scaleValue, scaleValue, scaleValue);


        GAME->Play_SoundCue(Constants::SoundCue::SFX_Eveque_Earth_SpikeHit, &spawnPos);


        CLIENT->Play_Effect(desc);
    }

}
