#include "AN_SL_PlayCachedPlayerEffect.h"

#include "ClientInstance.h"
#include "ContainerObject.h"
#include "GameObject.h"
#include "SL_Dualliste.h"

IMPLEMENT_REFLECTION(AN_SL_PlayCachedPlayerEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_PlayCachedPlayerEffect";
    info.category = "SoulLike";

    PROPERTY_WSTRING("Effect Name", _effectName);
    PROPERTY_WSTRING("Layer Tag", _layerTag);
    PROPERTY_VEC3("Local Offset", _localOffset);
    PROPERTY_VEC3("Scale", _scale);

    return true;
}

void AN_SL_PlayCachedPlayerEffect::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview || _effectName.empty())
        return;

    const Shared<SL_Dualliste> owner = Resolve_OwnerDualliste(context.owner);
    if (owner == nullptr)
        return;

    Vec3 cachedPlayerPos{};
    Vec3 cachedPlayerRight{};
    Vec3 cachedPlayerUp{};
    Vec3 cachedPlayerForward{};
    if (!owner->Try_GetCachedPlayerAnchor(
        cachedPlayerPos,
        cachedPlayerRight,
        cachedPlayerUp,
        cachedPlayerForward))
    {
        return;
    }

    PlayEffectDesc effectDesc{};
    effectDesc.effectName = _effectName;
    effectDesc.layerTag = _layerTag;
    effectDesc.worldPosition =
        cachedPlayerPos +
        cachedPlayerRight * _localOffset.x +
        cachedPlayerUp * _localOffset.y +
        cachedPlayerForward * _localOffset.z;
    effectDesc.scale = _scale;

    CLIENT->Play_Effect(effectDesc);
}

Shared<SL_Dualliste> AN_SL_PlayCachedPlayerEffect::Resolve_OwnerDualliste(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (const Shared<SL_Dualliste> owner = dynamic_pointer_cast<SL_Dualliste>(current))
            return owner;

        if (dynamic_pointer_cast<ContainerObject>(current) && current->Get_Owner() == nullptr)
            return dynamic_pointer_cast<SL_Dualliste>(current);

        current = current->Get_Owner();
    }

    return nullptr;
}
