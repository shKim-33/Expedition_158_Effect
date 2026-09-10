#include "TextureDisplayInfoResolver.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <unordered_map>

#include "GameInstance.h"
#include "Helper_String.h"

namespace EffectEditor::TextureDisplayInfoResolver
{
    namespace
    {
        //## Types::Cache
        struct TextureTableCache
        {
            bool loaded = false;
            fs::file_time_type lastWriteTime{};
            unordered_map<string, TextureDisplayInfo> byId{};
            unordered_map<wstring, string> idByResolvedPath{};
        };

        //## Static::TextureTable
        constexpr const wchar_t* kTextureTablePath = L"../../../Client/Bin/Resources/Data/json/DT_Texture.json";

        //## Data::Cache
        TextureTableCache cache{};

        //## Helper::TextAndPath
        string ToUpperCopy(string value)
        {
            ranges::transform(
                value,
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(::toupper(character));
                }
            );
            return value;
        }

        wstring ToLowerCopy(wstring value)
        {
            ranges::transform(
                value,
                value.begin(),
                [](wchar_t character)
                {
                    return static_cast<wchar_t>(::towlower(character));
                }
            );
            return value;
        }

        fs::path NormalizePath(const fs::path& path)
        {
            try
            {
                if (fs::exists(path))
                    return fs::weakly_canonical(path);
            }
            catch (...)
            {
            }

            try
            {
                return fs::absolute(path).lexically_normal();
            }
            catch (...)
            {
            }

            return path.lexically_normal();
        }

        fs::path ResolveEditorPath(const wchar_t* path)
        {
            return NormalizePath(fs::path(path));
        }

        fs::path ResolveTextureTablePath(const string& tablePathText)
        {
            string normalizedText = tablePathText;
            ranges::replace(normalizedText, '\\', '/');

            constexpr const char* marker = "Bin/Resources/";
            const size_t markerPos = normalizedText.find(marker);
            if (markerPos != string::npos)
            {
                const string resourceRelative = normalizedText.substr(markerPos + string{ marker }.size());
                fs::path resourceRoot = GAME ? fs::path(GAME->Get_AssetRoot()) : fs::path{};
                if (resourceRoot.empty())
                    resourceRoot = fs::path(L"../../../Client/Bin/Resources");
                return NormalizePath(resourceRoot / String::ToWString(resourceRelative));
            }

            return NormalizePath(String::ToWString(tablePathText));
        }

        string MakeFormatLabel(const fs::path& path)
        {
            string extension = path.extension().string();
            if (!extension.empty() && extension.front() == '.')
                extension.erase(extension.begin());
            return extension.empty() ? string{} : ToUpperCopy(extension);
        }

        //## Helper::TextureTableCache
        bool RefreshTextureTableCache()
        {
            const fs::path tablePath = ResolveEditorPath(kTextureTablePath);
            if (tablePath.empty() || !fs::exists(tablePath))
                return false;

            fs::file_time_type lastWriteTime{};
            try
            {
                lastWriteTime = fs::last_write_time(tablePath);
            }
            catch (...)
            {
                return false;
            }

            if (cache.loaded && cache.lastWriteTime == lastWriteTime)
                return true;

            ifstream file{ tablePath };
            if (!file.is_open())
                return false;

            json root{};
            try
            {
                file >> root;
            }
            catch (...)
            {
                return false;
            }

            const json* textureArray = nullptr;
            if (root.contains("DT_Texture"))
                textureArray = &root["DT_Texture"];
            else if (root.contains("Texture"))
                textureArray = &root["Texture"];

            if (textureArray == nullptr || !textureArray->is_array())
                return false;

            unordered_map<string, TextureDisplayInfo> byId{};
            unordered_map<wstring, string> idByResolvedPath{};

            for (const json& item : *textureArray)
            {
                const string id = item.value("Id", item.value("id", string{}));
                const string path = item.value("Path", item.value("path", string{}));
                if (id.empty() || path.empty() || path.find('%') != string::npos)
                    continue;

                const fs::path resolvedPath = ResolveTextureTablePath(path);
                TextureDisplayInfo info{};
                info.id = id;
                info.path = path;
                info.fileName = resolvedPath.filename().string();
                info.format = MakeFormatLabel(resolvedPath);

                byId.emplace(id, info);
                idByResolvedPath.emplace(ToLowerCopy(NormalizePath(resolvedPath).wstring()), id);
            }

            cache.loaded = true;
            cache.lastWriteTime = lastWriteTime;
            cache.byId = std::move(byId);
            cache.idByResolvedPath = std::move(idByResolvedPath);
            return true;
        }
    }

    //## Lookup::TextureInfo
    const TextureDisplayInfo* FindTextureInfoById(const string& textureId)
    {
        if (textureId.empty() || !RefreshTextureTableCache())
            return nullptr;

        const auto it = cache.byId.find(textureId);
        return it == cache.byId.end() ? nullptr : &it->second;
    }

    bool TryFindTextureIdByFilePath(const fs::path& filePath, string& outTextureId)
    {
        outTextureId.clear();

        if (!RefreshTextureTableCache())
            return false;

        const wstring selectedPath = ToLowerCopy(NormalizePath(filePath).wstring());
        const auto it = cache.idByResolvedPath.find(selectedPath);
        if (it == cache.idByResolvedPath.end())
            return false;

        outTextureId = it->second;
        return true;
    }

    //## Format::TextureInfo
    string FormatTextureId(const string& textureId)
    {
        if (textureId.empty())
            return "(none)";

        const TextureDisplayInfo* info = FindTextureInfoById(textureId);
        if (info == nullptr || info->format.empty())
            return textureId + " [Unknown]";

        return textureId + " [" + info->format + "]";
    }

    string FormatTextureIdOptional(const string& textureId)
    {
        return textureId.empty() ? string{} : FormatTextureId(textureId);
    }

    string MakeTextureTooltip(const string& textureId)
    {
        if (textureId.empty())
            return "Id: (none)";

        const TextureDisplayInfo* info = FindTextureInfoById(textureId);
        if (info == nullptr)
            return "Id: " + textureId + "\nFormat: Unknown\nPath: (not found in DT_Texture)";

        return
            "Id: " + textureId +
            "\nFormat: " + (info->format.empty() ? "Unknown" : info->format) +
            "\nPath: " + info->path;
    }
}
