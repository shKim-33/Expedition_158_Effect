#pragma once

#include "Editor_Window.h"
#include "ContentBrowserCore.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(EffectEditor)

class Content_Browser final : public Editor_Window, private ContentBrowserCore
{
public:
    Content_Browser();
    ~Content_Browser() override;

public:
    void Update(float timeDelta) override;
    void Render() override;
    void Request_RefreshResources();

private: //## Types::Navigation
    enum class IconType
    {
        Folder,
        Csv,
        Json,
        Python,
        Xlsx,
        End
    };

    using FileEntry = ContentBrowserFileEntry;
    using FolderNode = ContentBrowserFolderNode;
    using ThumbnailPolicy = ContentBrowserThumbnailPolicy;
    using GridItemRef = ContentBrowserGridItemRef;

private: //## Static::ContentBrowser
    static constexpr auto kContentBrowserSettingsPath{
        L"../../../Client/Bin/Resources/Data/json/EditorSettings/ContentBrowser.json" };

private: //## Data::ContentBrowser
    float _leftPanelWidth{ 300.f };
    float _thumbnailSize{ 64.f };
    char _searchBuffer[128]{};
    char _assetSearchBuffer[128]{};

    vector<Shared<Texture>> _iconFiles{};
    map<wstring, Shared<Texture>> _thumbnailCache{};
    set<wstring> _noThumbnailPaths{};
    ThumbnailPolicy _thumbnailPolicy{};
    int _thumbnailLoadsThisFrame{ 0 };
    int _thumbnailLoadBudgetThisFrame{ 4 };

    FolderNode* _gridItemCacheFolder{};
    size_t _gridItemCacheSubFolderCount{};
    size_t _gridItemCacheFileCount{};
    string _gridItemCacheSearchLower{};
    vector<GridItemRef> _gridItemCache{};

    bool _needsInitialRefresh{ true };
    bool _pendingResourceRefresh{ false };

    wstring _selectedFilePath{};

private: //## Helper::Refresh
    void Refresh_Resources();
    void Refresh_AssetState();
    void Refresh_CurrentFolder();
    void Load_Icons();

private: //## Helper::View
    void Draw_FolderTree(FolderNode& node);
    void Draw_AssetView();
    void Draw_ThumbnailSettingsPopup();
    void Draw_FolderTile(FolderNode& folder);
    void Open_Folder(FolderNode* folder);
    void Open_File(const FileEntry& fileEntry);
    void Draw_FolderContextMenu(const FolderNode& folder, const string& popupId);
    void Draw_FileContextMenu(const wstring& filePath, const string& popupId);

private: //## Helper::ThumbnailIcon
    Shared<Texture> Get_FileThumbnail(const FileEntry& fileEntry, bool allowLoad);
    static bool Is_EffectMaterialPresetFile(const fs::path& filePath);
    bool Is_LargeTextureFolder() const;
    int Resolve_CurrentThumbnailLoadBudget() const;

private: //## Helper::SettingsSearch
    void Save_Settings() const;
    void Load_Settings();
    IconType Resolve_IconType(const fs::path& filePath) const;
    ImTextureID Get_IconTextureId(IconType iconType) const;

private: //## Helper::NavigationHistory
    bool Can_GoParentFolder() const;
    void Go_ParentFolder();
    void Go_BackFolder();
    void Go_ForwardFolder();
    void Handle_SideButtonEvent();
    void Handle_ThumbnailZoom();

public:
    static Shared<Content_Browser> Create();
    void Free() override;
};

NS_END
