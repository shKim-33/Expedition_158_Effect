#pragma once

namespace EffectEditor::TextureDisplayInfoResolver
{
    //## Types::DisplayInfo
    struct TextureDisplayInfo
    {
        string id{};
        string path{};
        string fileName{};
        string format{};
    };

    //## Lookup::TextureInfo
    const TextureDisplayInfo* FindTextureInfoById(const string& textureId);
    bool TryFindTextureIdByFilePath(const fs::path& filePath, string& outTextureId);

    //## Format::TextureInfo
    string FormatTextureId(const string& textureId);
    string FormatTextureIdOptional(const string& textureId);
    string MakeTextureTooltip(const string& textureId);
}
