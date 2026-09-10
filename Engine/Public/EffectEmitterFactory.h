#pragma once

#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class EffectEmitter;

class ENGINE_DLL EffectEmitterFactory final
{
public: //## Types::Creation
    using CreateEmitterFn = Shared<EffectEmitter>(*)(const EffectEmitterDefinition& emitterDefinition);

public: //## Behavior::Creation
    static void Register_CreateEmitterFn(CreateEmitterFn createEmitterFn);
    static Shared<EffectEmitter> Create_EffectEmitter(const EffectEmitterDefinition& emitterDefinition);
};

NS_END
