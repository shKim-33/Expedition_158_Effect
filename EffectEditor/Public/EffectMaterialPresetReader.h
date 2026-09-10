#pragma once

#include "EffectMaterialAuthoring_Types.h"

namespace EffectEditor::EffectMaterialPresetReader
{
bool Read(const fs::path& filePath, EffectMaterialInstanceData& outMaterial);

string ResolveDisplayName(const fs::path& filePath);
}
