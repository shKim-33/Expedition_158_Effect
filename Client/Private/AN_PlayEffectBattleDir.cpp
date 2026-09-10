#include "AN_PlayEffectBattleDir.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Monster_Part.h"
#include "PlayableParts.h"
#include "TransformCom.h"
#include "Weapon.h"

namespace
{
Shared<GameObject> Resolve_NotifyOwnerRoot(GameObject* notifyOwner)
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (dynamic_pointer_cast<ContainerObject>(current))
            return current;

        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

bool Try_GetOwnerRootWorldMatrix(const Shared<GameObject>& ownerObject, Matrix& outWorldMatrix)
{
    if (ownerObject == nullptr)
        return false;

    if (const Shared<PartObject> ownerPart = dynamic_pointer_cast<PartObject>(ownerObject))
    {
        outWorldMatrix = ownerPart->Get_CombinedWorldMatrix();
        return true;
    }

    const Shared<TransformCom> ownerTransform = ownerObject->Get_Transform();
    if (ownerTransform == nullptr)
        return false;

    outWorldMatrix = ownerTransform->Get_WorldMatrix();
    return true;
}

bool Try_GetSocketWorldMatrix(
    const Shared<GameObject>& ownerObject,
    const wstring& partTag,
    const string& socketName,
    Matrix& outWorldMatrix)
{
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    if (const auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        outWorldMatrix = *socketMatrix * playablePart->Get_CombinedWorldMatrix();
        return true;
    }

    if (const auto* monsterPart = dynamic_cast<Monster_Part*>(partObject))
    {
        const Matrix* socketMatrix = monsterPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        outWorldMatrix = *socketMatrix * monsterPart->Get_CombinedWorldMatrix();
        return true;
    }

    if (const auto* weaponPart = dynamic_cast<Weapon*>(partObject))
    {
        const Matrix* socketMatrix = weaponPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        outWorldMatrix = *socketMatrix * weaponPart->Get_CombinedWorldMatrix();
        return true;
    }

    return false;
}

bool Try_GetPartRootWorldMatrix(
    const Shared<GameObject>& ownerObject,
    const wstring& partTag,
    Matrix& outWorldMatrix)
{
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    outWorldMatrix = partObject->Get_CombinedWorldMatrix();
    return true;
}

bool Try_ResolveNotifyBaseWorldMatrix(
    const Shared<GameObject>& ownerObject,
    EffectCom::EffectPlayTarget target,
    const wstring& partTag,
    const string& socketName,
    Matrix& outWorldMatrix)
{
    if (target == EffectCom::EffectPlayTarget::OwnerRoot)
        return Try_GetOwnerRootWorldMatrix(ownerObject, outWorldMatrix);

    if (target == EffectCom::EffectPlayTarget::PartSocket)
        return Try_GetSocketWorldMatrix(ownerObject, partTag, socketName, outWorldMatrix);

    return Try_GetPartRootWorldMatrix(ownerObject, partTag, outWorldMatrix);
}

const BattleChar* Resolve_CurrentBattleTarget(BattleController& battle)
{
    const vector<BattleChar>& currentStepTargets = battle.Get_TurnAction().GetCurStepTarget();
    for (const BattleChar& target : currentStepTargets)
    {
        if (target.actor != nullptr && !target.actor->Is_MarkDestroy())
            return &target;
    }

    const BattleChar& initTarget = battle.Get_TurnAction().GetInitTarget();
    if (initTarget.actor != nullptr && !initTarget.actor->Is_MarkDestroy())
        return &initTarget;

    return nullptr;
}

bool Try_ResolveCurrentBattleContext(
    const AnimNotifyContext& context,
    Shared<GameObject>& outNotifyOwnerRoot,
    Shared<BattleController>& outBattle)
{
    outNotifyOwnerRoot = nullptr;
    outBattle = nullptr;

    if (context.isPreview || context.owner == nullptr)
        return false;

    outNotifyOwnerRoot = Resolve_NotifyOwnerRoot(context.owner);
    if (outNotifyOwnerRoot == nullptr)
        return false;

    outBattle = CLIENT->Get_BattleController();
    if (outBattle == nullptr || !outBattle->Is_BattleStarted())
        return false;

    const BattleChar& currentActor = outBattle->Get_TurnAction().Get_CurChar();
    if (currentActor.actor == nullptr || currentActor.actor.get() != outNotifyOwnerRoot.get())
        return false;

    return true;
}

vector<BattleChar> Resolve_EffectBattleTargets(const AnimNotifyContext& context)
{
    Shared<GameObject> notifyOwnerRoot{};
    Shared<BattleController> battle{};
    if (!Try_ResolveCurrentBattleContext(context, notifyOwnerRoot, battle))
        return {};

    const BattleAction& turnAction = battle->Get_TurnAction();
    if (turnAction.GetCurStepIndex() >= turnAction.GetSteps().size())
        return {};

    const BattleAction::AttackStep& currentStep = turnAction.GetCurStep();
    const bool isAllEnemiesStep =
        currentStep.targetType == TargetType::AllEnemies ||
        currentStep.targetType == TargetType::AllEnemiesJump;

    if (isAllEnemiesStep)
    {
        if (turnAction.Get_CurChar().statCom && turnAction.Get_CurChar().statCom->Is_Player())
            return battle->Get_BattleEnemies();

        return battle->Get_BattleHeroes();
    }

    vector<BattleChar> resolvedTargets{};
    for (const BattleChar& target : turnAction.GetCurStepTarget())
    {
        if (target.actor != nullptr && !target.actor->Is_MarkDestroy())
            resolvedTargets.push_back(target);
    }

    if (!resolvedTargets.empty())
        return resolvedTargets;

    const BattleChar& initTarget = turnAction.GetInitTarget();
    if (initTarget.actor != nullptr && !initTarget.actor->Is_MarkDestroy())
        resolvedTargets.push_back(initTarget);

    return resolvedTargets;
}

Vec3 NormalizeAxis(Vec3 axis, const Vec3& fallback)
{
    if (axis.LengthSquared() <= 0.000001f)
        return fallback;

    axis.Normalize();
    return axis;
}

bool Try_BuildEffectBasisWorldMatrix(const Matrix& baseWorldMatrix, Matrix& outBasisWorldMatrix)
{
    Vec3 baseScale = Vec3::One;
    Quat baseRotation = Quat::Identity;
    Vec3 baseTranslation = Vec3::Zero;
    Matrix decomposableBaseWorldMatrix = baseWorldMatrix;
    if (!decomposableBaseWorldMatrix.Decompose(baseScale, baseRotation, baseTranslation))
        return false;

    const Vec3 basisRight = NormalizeAxis(
        Vec3(baseWorldMatrix._11, baseWorldMatrix._12, baseWorldMatrix._13),
        Vec3::Right);
    const Vec3 basisUp = NormalizeAxis(
        Vec3(baseWorldMatrix._21, baseWorldMatrix._22, baseWorldMatrix._23),
        Vec3::Up);
    const Vec3 basisLook = NormalizeAxis(
        Vec3(baseWorldMatrix._31, baseWorldMatrix._32, baseWorldMatrix._33),
        Vec3::Look);

    outBasisWorldMatrix = Matrix::Identity;
    outBasisWorldMatrix._11 = basisRight.x;
    outBasisWorldMatrix._12 = basisRight.y;
    outBasisWorldMatrix._13 = basisRight.z;
    outBasisWorldMatrix._21 = basisUp.x;
    outBasisWorldMatrix._22 = basisUp.y;
    outBasisWorldMatrix._23 = basisUp.z;
    outBasisWorldMatrix._31 = basisLook.x;
    outBasisWorldMatrix._32 = basisLook.y;
    outBasisWorldMatrix._33 = basisLook.z;
    outBasisWorldMatrix._41 = baseTranslation.x;
    outBasisWorldMatrix._42 = baseTranslation.y;
    outBasisWorldMatrix._43 = baseTranslation.z;
    return true;
}

bool Try_ApplyBattleTargetFacing(
    const AnimNotifyContext& context,
    const EffectCom::EffectPlayDesc& sourceDesc,
    EffectCom::EffectPlayDesc& inOutDesc,
    const BattleChar* explicitTarget = nullptr)
{
    Shared<GameObject> notifyOwnerRoot{};
    Shared<BattleController> battle{};
    if (!Try_ResolveCurrentBattleContext(context, notifyOwnerRoot, battle))
        return false;

    const BattleChar* currentTarget = explicitTarget ? explicitTarget : Resolve_CurrentBattleTarget(*battle);
    if (currentTarget == nullptr || currentTarget->actor == nullptr || currentTarget->actor->Get_Transform() == nullptr)
        return false;

    Matrix baseWorldMatrix = Matrix::Identity;
    if (!Try_ResolveNotifyBaseWorldMatrix(
        notifyOwnerRoot,
        sourceDesc.target,
        sourceDesc.partTag,
        sourceDesc.socketName,
        baseWorldMatrix))
    {
        return false;
    }

    Matrix basisWorldMatrix = Matrix::Identity;
    if (!Try_BuildEffectBasisWorldMatrix(baseWorldMatrix, basisWorldMatrix))
        return false;

    const Vec3 aimOrigin = Vec3::Transform(sourceDesc.localOffset, basisWorldMatrix);
    Vec3 targetDirection = currentTarget->actor->Get_Transform()->Get_WorldPosition() - aimOrigin;
    if (targetDirection.LengthSquared() <= 0.000001f)
        return false;

    targetDirection.Normalize();

    Vec3 upAxis = Vec3::Up;
    if (abs(targetDirection.Dot(upAxis)) > 1.f - 1e-3f)
        upAxis = Vec3::Look;

    Vec3 desiredRight = upAxis.Cross(targetDirection);
    desiredRight.Normalize();
    Vec3 desiredUp = targetDirection.Cross(desiredRight);
    desiredUp.Normalize();

    Matrix desiredWorldRotation = Matrix::Identity;
    desiredWorldRotation._11 = desiredRight.x;
    desiredWorldRotation._12 = desiredRight.y;
    desiredWorldRotation._13 = desiredRight.z;
    desiredWorldRotation._21 = desiredUp.x;
    desiredWorldRotation._22 = desiredUp.y;
    desiredWorldRotation._23 = desiredUp.z;
    desiredWorldRotation._31 = targetDirection.x;
    desiredWorldRotation._32 = targetDirection.y;
    desiredWorldRotation._33 = targetDirection.z;

    basisWorldMatrix._41 = 0.f;
    basisWorldMatrix._42 = 0.f;
    basisWorldMatrix._43 = 0.f;
    Matrix desiredLocalRotation = desiredWorldRotation * basisWorldMatrix.Invert();

    Vec3 localScale = Vec3::One;
    Quat localRotation = Quat::Identity;
    Vec3 localTranslation = Vec3::Zero;
    if (!desiredLocalRotation.Decompose(localScale, localRotation, localTranslation))
        return false;

    localRotation.Normalize();
    const Vec3 localEuler = localRotation.ToEuler();
    inOutDesc.localRotation = Vec3(
        XMConvertToDegrees(localEuler.x),
        XMConvertToDegrees(localEuler.y),
        XMConvertToDegrees(localEuler.z));
    return true;
}
}

IMPLEMENT_REFLECTION(AN_PlayEffectBattleDir)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_WSTRING("Effect Name", _effectName);
    PROPERTY_ENUM("Target", _target, EffectCom::EffectPlayTarget);
    PROPERTY_WSTRING("Part Tag", _partTag);
    PROPERTY_STRING("Socket Name", _socketName);
    PROPERTY_WSTRING("Layer Tag", _layerTag);
    PROPERTY_VEC3("Local Offset", _localOffset);
    PROPERTY_VEC3("Local Rotation", _localRotation);
    PROPERTY_VEC3("Scale", _scale);
    PROPERTY_FLOAT("Playback Speed", _playbackSpeed, 0.f, 10.f, 0.01f);

    return true;
}

void AN_PlayEffectBattleDir::Execute(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(context.owner, _target);
    if (effectCom == nullptr)
        return;

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _effectName;
    desc.target = _target;
    desc.partTag = _partTag;
    desc.socketName = _socketName;
    desc.layerTag = _layerTag;
    desc.localOffset = _localOffset;
    desc.localRotation = _localRotation;
    desc.scale = _scale;
    desc.playbackSpeed = _playbackSpeed;
    desc.transformInheritance.inheritPosition = true;
    desc.transformInheritance.inheritRotation = true;
    desc.transformInheritance.inheritScale = false;
    desc.followTarget = false;

    if (context.isPreview)
    {
        EffectInstancePool::AttachedHandle handle{};
        effectCom->Play_Effect(desc, &handle);
        return;
    }

    const vector<BattleChar> effectTargets = Resolve_EffectBattleTargets(context);

    if (effectTargets.empty())
    {
        Try_ApplyBattleTargetFacing(context, desc, desc);
        effectCom->Play_Effect(desc);
        return;
    }

    for (const BattleChar& target : effectTargets)
    {
        if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
            continue;

        EffectCom::EffectPlayDesc perTargetDesc = desc;
        Try_ApplyBattleTargetFacing(context, desc, perTargetDesc, &target);
        effectCom->Play_Effect(perTargetDesc);
    }
}
