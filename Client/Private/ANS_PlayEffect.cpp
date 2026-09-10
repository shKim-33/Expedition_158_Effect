#include "ANS_PlayEffect.h"

#include "EffectCom.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(ANS_PlayEffect)
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

void ANS_PlayEffect::On_Begin(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner, _target);
    if (effectCom == nullptr)
        return;

    if (_activeEffectHandle.effectObject != nullptr)
        effectCom->Stop_Effect(_activeEffectHandle);

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
    desc.followTarget = true; // ANS는 EffectCom::_attachedEffects에서 계속 follow한다.

    _activeEffectHandle = {};
    effectCom->Play_Effect(desc, &_activeEffectHandle);
}

void ANS_PlayEffect::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_PlayEffect::On_End(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner, _target);
    if (effectCom == nullptr)
        return;

    effectCom->Stop_Effect(_activeEffectHandle);
    _activeEffectHandle = {};
}
