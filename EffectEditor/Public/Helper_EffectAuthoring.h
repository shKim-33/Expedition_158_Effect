#pragma once

#include "EffectAuthoring_Types.h"

namespace EffectEditor::Authoring
{
inline const char* Resolve_RendererType(const AuthoringTypeData& typeData)
{
    switch (typeData.kind)
    {
    case AuthoringTypeDataKind::Trail:
        return "Trail";

    case AuthoringTypeDataKind::Mesh:
        return "Mesh";

    case AuthoringTypeDataKind::Ribbon:
        return "Ribbon";

    case AuthoringTypeDataKind::SourceHistorySpriteTrail:
        return "SourceHistorySpriteTrail";

    case AuthoringTypeDataKind::Beam:
        return "Beam";

    case AuthoringTypeDataKind::None:
    default:
        return "Sprite";
    }
}
}
