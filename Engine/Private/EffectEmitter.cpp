#include "EffectEmitter.h"

EffectEmitter::EffectEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectEmitter::EffectEmitter(const EffectEmitter& prototype)
    : GameObject{ prototype }
    , _effectOwner{ prototype._effectOwner }
    , _emitterId{ prototype._emitterId }
    , _emitterName{ prototype._emitterName }
    , _localPosition{ prototype._localPosition }
    , _localRotationDegrees{ prototype._localRotationDegrees }
    , _useLocalSpace{ prototype._useLocalSpace }
    , _enabled{ prototype._enabled }
    , _effectPlaybackSeed{ prototype._effectPlaybackSeed }
{
}

HRESULT EffectEmitter::Attach_ToEffect(const EffectEmitterRuntimeDesc& runtimeDesc)
{
    _effectOwner = runtimeDesc.effectOwner;
    _emitterId = runtimeDesc.emitterId;
    _emitterName = runtimeDesc.emitterName;
    _localPosition = runtimeDesc.localPosition;
    _localRotationDegrees = runtimeDesc.localRotationDegrees;
    _useLocalSpace = runtimeDesc.useLocalSpace;
    _enabled = runtimeDesc.enabled;
    _effectPlaybackSeed = runtimeDesc.effectPlaybackSeed;

    if (runtimeDesc.effectOwner)
        Set_Owner(runtimeDesc.effectOwner);

    Sync_FromEffectOwner();

    return S_OK;
}

void EffectEmitter::Sync_FromEffectOwner()
{
    CHECK_NULL_THROTTLED(_transformCom, 120);

    const Shared<GameObject> effectOwner = _effectOwner.lock();
    const Shared<TransformCom> ownerTransform =
        effectOwner != nullptr ? effectOwner->Get_Transform() : Shared<TransformCom>{};

    if (ownerTransform)
    {
        _transformCom->Set_Parent(ownerTransform);
        _transformCom->Set_Position(_localPosition);
        _transformCom->Set_RotationEuler(_localRotationDegrees);
        _transformCom->Set_Scale(Vec3::One);
    }
    else
    {
        _transformCom->Set_Parent(nullptr);
        _transformCom->Set_WorldPosition(_localPosition);
        _transformCom->Set_WorldRotationEuler(_localRotationDegrees);
    }

    On_EffectTransformSynced();
}

void EffectEmitter::Set_EffectPlaybackSeed(uint32 effectPlaybackSeed)
{
    _effectPlaybackSeed = effectPlaybackSeed;
}

void EffectEmitter::Set_RuntimeLocalTransform(const Vec3& localPosition, const Vec3& localRotationDegrees)
{
    _localPosition = localPosition;
    _localRotationDegrees = localRotationDegrees;
    Sync_FromEffectOwner();
}

HRESULT EffectEmitter::Reset_ForEffectReplay()
{
    return S_OK;
}

bool EffectEmitter::Can_ReloadDefinition(const EffectEmitterDefinition&) const
{
    return false;
}

HRESULT EffectEmitter::Reload_Definition(const EffectEmitterDefinition&)
{
    return E_FAIL;
}

bool EffectEmitter::Is_EffectFinished() const
{
    return false;
}

void EffectEmitter::Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>&)
{
}

void EffectEmitter::Bind_SourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>&)
{
}

void EffectEmitter::Bind_RibbonSourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>&)
{
}

bool EffectEmitter::Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>&, uint32) const
{
    return false;
}

void EffectEmitter::Set_MaterialRevealOverride(float, float)
{
}

void EffectEmitter::Set_MaterialTintOverride(const Vec4&, float)
{
}

bool EffectEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (_transformCom == nullptr)
        return false;

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

EffectSortPolicy EffectEmitter::Get_BlendSortPolicy() const
{
    return EffectSortPolicy::EmitterDepth;
}

int32 EffectEmitter::Get_BlendSortLayer() const
{
    return 0;
}

float EffectEmitter::Get_BlendSortBias() const
{
    return 0.f;
}
