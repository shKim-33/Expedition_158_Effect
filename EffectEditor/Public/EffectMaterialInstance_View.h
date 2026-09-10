#pragma once
#include "Editor_Window.h"

#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "Emitter_View.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(EffectEditor)

class EffectMaterial_View;
class EffectMaterialPreviewRenderer;

enum class EffectMaterialTwoSidedDisplayMode : uint8
{
    EditableImplemented,
    EditableUnimplemented,
    ForcedEnabledReadOnly,
};

enum class EffectMaterialDistortionShapeControlMode : uint8
{
    Editable,
    TrailEditable,
    MeshUnsupported,
    BeamUnsupported,
};

struct EffectMaterialRenderPolicyBinding
{
    EffectMaterialBlendMode blendMode{ EffectMaterialBlendMode::AlphaBlend };
    float alphaCutoff{ 0.5f };
    const char* sourceLabel{ "Required" };
};

class EffectMaterialInstance_View final : public Editor_Window
{
public:
    EffectMaterialInstance_View();
    ~EffectMaterialInstance_View() override;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;
    static bool Draw_InstanceProperties(
        DetailPropertyContext& detailPropertyContext,
        map<string, Shared<Texture>>& textureThumbnailCache,
        EffectMaterialInstanceData& material,
        bool* outRestartPreview,
        EffectMaterialTwoSidedDisplayMode twoSidedDisplayMode = EffectMaterialTwoSidedDisplayMode::EditableImplemented,
        EffectMaterialDistortionShapeControlMode distortionShapeControlMode = EffectMaterialDistortionShapeControlMode::Editable,
        const EffectMaterialRenderPolicyBinding* renderPolicyBinding = nullptr,
        const char* mainTextureSourceLockTooltip = nullptr);

public: //## Behavior::Selection
    void Open_Instance(uint32 emitterId, uint32 moduleId);

private: //## Data::Target
    uint32 _targetEmitterId{ 0 };
    uint32 _targetModuleId{ 0 };
    DetailPropertyContext _detailPropertyContext{};
    Unique<EffectMaterialPreviewRenderer> _previewRenderer{};
    map<string, Shared<Texture>> _textureThumbnailCache{};

private: //## Data::AuthoringHistory
    bool _hasPendingAuthoringEdit{ false };
    EffectAuthoringSelection _pendingAuthoringSelection{};
    Emitter_View::AuthoringSnapshot _pendingAuthoringSnapshot{};
    string _pendingAuthoringDescription{};

private: //## Helper::MaterialInstance
    bool Resolve_Target(Shared<Emitter_View>& outEmitterView, AuthoringEmitter*& outEmitter, RequiredModuleData*& outRequiredData) const;
    void Apply_MaterialChanged(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, const RequiredModuleData& requiredData);
    void Begin_PendingAuthoringEdit(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        const Emitter_View::AuthoringSnapshot& beforeSnapshot,
        const string& description);
    void Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView);
    void Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView);

public:
    static Shared<EffectMaterialInstance_View> Create();
};

NS_END
