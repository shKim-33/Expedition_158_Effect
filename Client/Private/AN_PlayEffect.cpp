#include "AN_PlayEffect.h"

#include "EffectCom.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(AN_PlayEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("이펙트 이름", _effectName);
    PROPERTY_ENUM("Target", _target, EffectCom::EffectPlayTarget);
    PROPERTY_WSTRING("파츠 이름", _partTag);
    PROPERTY_STRING("소켓 이름", _socketName);
    PROPERTY_WSTRING("레이어 태그", _layerTag);
    PROPERTY_VEC3("로컬 오프셋", _localOffset);
    PROPERTY_VEC3("로컬 회전", _localRotation);
    PROPERTY_VEC3("스케일", _scale);
    PROPERTY_FLOAT("이펙트 재생 속도", _playbackSpeed, 0.f, 10.f, 0.01f);
    PROPERTY_BOOL("위치 상속", _inheritPosition);
    PROPERTY_BOOL("회전 상속", _inheritRotation);
    PROPERTY_BOOL("스케일 상속", _inheritScale);
    return true;
}

void AN_PlayEffect::Execute(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner, _target);
    if (effectCom == nullptr)
        return;

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _effectName;
    desc.target = _target;
    desc.partTag = _partTag;
    desc.socketName = _socketName;
    desc.layerTag = _layerTag;
    desc.localOffset = _localOffset;
    desc.localRotation = _localRotation;
    desc.scale = _scale;
    desc.playbackSpeed = _playbackSpeed;
    desc.transformInheritance.inheritPosition = _inheritPosition;
    desc.transformInheritance.inheritRotation = _inheritRotation;
    desc.transformInheritance.inheritScale = _inheritScale;
    desc.followTarget = false; // AN은 시작 transform만 계산하고 attach하지 않는다.

    if (context.isPreview)
    {
        EffectInstancePool::AttachedHandle handle{};
        effectCom->Play_Effect(desc, &handle);
        return;
    }

    effectCom->Play_Effect(desc);
}
