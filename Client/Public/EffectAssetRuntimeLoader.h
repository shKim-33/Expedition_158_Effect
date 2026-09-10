#pragma once
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

class EffectAssetRuntimeLoader
{
public:
    EffectAssetRuntimeLoader() = delete;

public: //## Behavior::Load
    static Shared<const EffectDefinition> Load_Definition(const wstring& effectAssetPath);
    static Shared<const EffectDefinition> Load_DefinitionByName(const wstring& effectName);
};

NS_END
