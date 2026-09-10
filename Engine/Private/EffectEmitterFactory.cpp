#include "EffectEmitterFactory.h"

#include "EffectEmitter.h"

NS_BEGIN(Engine)

namespace
{
    EffectEmitterFactory::CreateEmitterFn gCreateEmitterFn = nullptr;
}

void EffectEmitterFactory::Register_CreateEmitterFn(CreateEmitterFn createEmitterFn)
{
    gCreateEmitterFn = createEmitterFn;
}

Shared<EffectEmitter> EffectEmitterFactory::Create_EffectEmitter(const EffectEmitterDefinition& emitterDefinition)
{
    if (nullptr == gCreateEmitterFn)
    {
        LOG_ERROR("EffectEmitterFactory create hook is not registered.");
        return nullptr;
    }

    return gCreateEmitterFn(emitterDefinition);
}

NS_END
