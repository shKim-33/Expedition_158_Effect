#pragma once

#include "Editor_Window.h"
#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "Emitter_View.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(EffectEditor)

class MeshDataPreview_View final : public Editor_Window
{
public:
    MeshDataPreview_View();
    ~MeshDataPreview_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override;
    void Open_Target(uint32 emitterId);

private: //## Types::PreviewTarget
    struct PreviewRenderer;

private: //## Data::Preview
    Unique<PreviewRenderer> _previewRenderer{};
    DetailPropertyContext _detailPropertyContext{};
    map<string, Shared<Texture>> _textureThumbnailCache{};
    uint32 _targetEmitterId{};
    float _timeDelta{};
    bool _hasPendingAuthoringEdit{ false };
    Emitter_View::AuthoringSnapshot _pendingAuthoringSnapshot{};
    string _pendingAuthoringDescription{};

private: //## Helper::Target
    Shared<Emitter_View> Resolve_EmitterView() const;
    AuthoringEmitter* Find_TargetEmitter(const Shared<Emitter_View>& emitterView) const;
    MeshTypeData* Find_TargetMeshData(AuthoringEmitter* emitter) const;
    fs::path Resolve_AuthoredAssetPath(const string& guid, const string& path) const;
    void Render_Inspector(Emitter_View& emitterView, AuthoringEmitter& emitter, MeshTypeData& meshData);
    bool Render_ModelAssetSection(MeshTypeData& meshData);
    bool Render_MaterialAssetSection(MeshTypeData& meshData);
    bool Render_TransformSection(MeshTypeData& meshData);
    bool Render_MaterialInstanceSection(const AuthoringEmitter& emitter, MeshTypeData& meshData, bool* outRestartPreview);
    void Render_ValidationSummary(const MeshTypeData& meshData);
    void Apply_MeshDataChanged(Emitter_View& emitterView, AuthoringEmitter& emitter);

private: //## Helper::AuthoringHistory
    void Begin_PendingAuthoringEdit(
        Emitter_View& emitterView,
        const Emitter_View::AuthoringSnapshot& beforeSnapshot,
        const string& description);
    void Commit_PendingAuthoringEditIfIdle(Emitter_View& emitterView);
    void Commit_PendingAuthoringEdit(Emitter_View& emitterView);

public:
    static Shared<MeshDataPreview_View> Create();
    void Free() override;
};

NS_END
