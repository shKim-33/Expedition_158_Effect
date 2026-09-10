#pragma once

#include <string_view>
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

struct AuthoringValueRange
{
    bool hasMin{};
    bool hasMax{};
    float minValue{};
    float maxValue{};
};

const AuthoringValueRange* Find_AuthoringValueRange(AuthoringModuleType moduleType, std::string_view fieldId);

NS_END
