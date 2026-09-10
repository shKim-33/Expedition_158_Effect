#include "AN_SimonPlayEffectToggle.h"

#include "MB_Simon.h"

IMPLEMENT_REFLECTION(AN_SimonPlayEffectToggle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BOOL("playEffect", _playEffect);

    return true;
}

Client::MB_Simon* Resolve_SimonOwner(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return nullptr;

    Shared<GameObject> current = context.owner->GetSharedPtr<GameObject>();
    while (current)
    {
        if (auto* simon = dynamic_cast<Client::MB_Simon*>(current.get()))
            return simon;

        current = current->Get_Owner();
    }

    return nullptr;
}

void AN_SimonPlayEffectToggle::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (MB_Simon* simon = Resolve_SimonOwner(context))
        simon->Set_PlayEffect(_playEffect);
}
