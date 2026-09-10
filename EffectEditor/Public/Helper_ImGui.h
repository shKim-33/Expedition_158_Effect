#pragma once

#include "EffectEditor_Define.h"

NS_BEGIN(EffectEditor)

inline ImVec4 To_ImVec4(const Color& color)
{
    return ImVec4{ color.x, color.y, color.z, color.w };
}

NS_END
