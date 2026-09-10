#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(EffectEditor)

struct MaterialScalarModulationPreviewResult
{
    EffectMaterialInstanceData material{};
    bool applied{ false };
    bool skippedParticleLife{ false };
};

using MaterialParameterModulationPreviewResult = MaterialScalarModulationPreviewResult;

const RequiredModuleData* Find_RequiredModuleData(const AuthoringEmitter& emitter);
const MaterialScalarModulationModuleData* Find_MaterialScalarModulationModuleData(const AuthoringEmitter& emitter);
EffectMaterialScalarModulationRuntimeDesc Build_MaterialScalarModulationRuntimeDesc(const MaterialScalarModulationModuleData& data);
EffectMaterialCoreColorRgbModulationRuntimeDesc Build_CoreColorRgbModulationRuntimeDesc(const MaterialScalarModulationModuleData& data);
EffectMaterialVec2ModulationRuntimeDesc Build_MaterialVec2ModulationRuntimeDesc(const MaterialScalarModulationModuleData& data);
float Compute_MaterialScalarPreviewPhase(float elapsedTime, float duration);
MaterialParameterModulationPreviewResult Evaluate_MaterialParameterModulationPreview(
    const EffectMaterialInstanceData& baseMaterial,
    const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
    const EffectMaterialVec2ModulationRuntimeDesc& vec2Modulation,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewPhase,
    uint32 effectPlaybackSeed,
    bool evaluateParticleLife);
MaterialScalarModulationPreviewResult Evaluate_MaterialScalarModulationPreview(
    const EffectMaterialInstanceData& baseMaterial,
    const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
    const EffectMaterialVec2ModulationRuntimeDesc& vec2Modulation,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewPhase,
    uint32 effectPlaybackSeed,
    bool evaluateParticleLife);

NS_END
