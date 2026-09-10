#include "ModuleDetail_Drawers.h"

#include "EffectEditorInstance.h"
#include "EffectEditor_Macro.h"
#include "EffectMaterialInstance_View.h"
#include "EffectMaterialPresetReader.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectMaterial_View.h"
#include "Helper_EffectAuthoring.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr auto kMaterialDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Materials";
    constexpr float kMaterialFloatEpsilon = 1e-4f;

    bool Uses_DirectionalAlignmentOptions(EmitterScreenAlignment alignment)
    {
        return alignment == EmitterScreenAlignment::AwayFromCenter ||
               alignment == EmitterScreenAlignment::Velocity;
    }

    bool Uses_SourceHistorySpriteTrailCardAxis(EmitterScreenAlignment alignment, EmitterDirectionalAlignmentMode mode)
    {
        return alignment == EmitterScreenAlignment::FacingCameraPosition ||
               alignment == EmitterScreenAlignment::Rectangle ||
               alignment == EmitterScreenAlignment::WorldUpFacingCamera ||
               (alignment == EmitterScreenAlignment::Velocity && mode == EmitterDirectionalAlignmentMode::TextureAxis);
    }

    void Notify_DirectionalAlignmentModeChanged(EmitterScreenAlignment alignment, EmitterDirectionalAlignmentMode mode)
    {
        if (!Uses_DirectionalAlignmentOptions(alignment))
            return;

        const char* directionName = alignment == EmitterScreenAlignment::AwayFromCenter ? "Away from Center" : "Velocity";
        const char* modeHelp = mode == EmitterDirectionalAlignmentMode::LookDirection
                               ? "기준 방향을 sprite 카드의 바라보는 방향으로 사용합니다."
                               : "texture 축을 기준 방향에 맞추고 나머지 축은 카메라 가시성을 유지합니다.";

        NOTIFY("{}: {}", directionName, modeHelp);
    }

    void Notify_TextureAxisChanged(EmitterSpriteTextureAxis axis)
    {
        NOTIFY(
            "{}",
            axis == EmitterSpriteTextureAxis::X
            ? "Texture X: 정렬 방향에 texture의 가로축을 맞춥니다."
            : "Texture Y: 정렬 방향에 texture의 세로축을 맞춥니다."
        );
    }

    fs::path Normalize_MaterialDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {}

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {}

        return filePath.lexically_normal();
    }

    bool NearlyEqual(float lhs, float rhs)
    {
        return abs(lhs - rhs) <= kMaterialFloatEpsilon;
    }

    bool Is_SameColor(const Color& lhs, const Color& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z) &&
            NearlyEqual(lhs.w, rhs.w);
    }

    const char* Get_RibbonSpreadBasisLabel(RibbonSpreadBasis value)
    {
        switch (value)
        {
        case RibbonSpreadBasis::CameraFacing:
            return "카메라 바라보기";
        case RibbonSpreadBasis::ViewUp:
            return "화면 위쪽";
        case RibbonSpreadBasis::WorldUp:
            return "월드 위쪽";
        case RibbonSpreadBasis::SourceUp:
            return "소스 위쪽";
        case RibbonSpreadBasis::SourceRight:
            return "소스 오른쪽";
        default:
            return "카메라 바라보기";
        }
    }

    bool Draw_RibbonSpreadBasisProperty(
        DetailPropertyContext& detailContext,
        RibbonSpreadBasis& value,
        RibbonSpreadBasis defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RibbonSpreadBasis");
        detailContext.Draw_PropertyLabel("펼침 기준", "리본 중심선을 기준으로 폭 방향을 계산할 때 사용할 기준축입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_RibbonSpreadBasisLabel(value)))
        {
            constexpr RibbonSpreadBasis kValues[] = {
                RibbonSpreadBasis::CameraFacing,
                RibbonSpreadBasis::ViewUp,
                RibbonSpreadBasis::WorldUp,
                RibbonSpreadBasis::SourceUp,
                RibbonSpreadBasis::SourceRight,
            };

            for (const RibbonSpreadBasis candidate : kValues)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_RibbonSpreadBasisLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Is_SameAdditiveContribution(
        const EffectMaterialAdditiveContributionData& lhs,
        const EffectMaterialAdditiveContributionData& rhs)
    {
        return
            lhs.colorSource == rhs.colorSource &&
            lhs.amountSource == rhs.amountSource &&
            lhs.coveragePolicy == rhs.coveragePolicy &&
            NearlyEqual(lhs.intensityScale, rhs.intensityScale) &&
            lhs.blackNeutral == rhs.blackNeutral &&
            Is_SameColor(lhs.emissiveColor, rhs.emissiveColor) &&
            Is_SameColor(lhs.constantColor, rhs.constantColor);
    }

    bool Is_SameVec2(const Vec2& lhs, const Vec2& rhs)
    {
        return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
    }

    const char* Get_SubUVFramePlaybackModeLabel(SubUVFramePlaybackMode mode)
    {
        switch (mode)
        {
        case SubUVFramePlaybackMode::FixedFrame:
            return "고정 프레임";
        case SubUVFramePlaybackMode::LifeProgress:
            return "수명 맞춤 재생";
        case SubUVFramePlaybackMode::FramesPerSecond:
            return "초당 프레임 재생";
        case SubUVFramePlaybackMode::RandomFrame:
            return "랜덤 프레임";
        default:
            return "고정 프레임";
        }
    }

    const char* Get_DistributionRandomSeedModeLabel(DistributionRandomSeedMode mode)
    {
        switch (mode)
        {
        case DistributionRandomSeedMode::Default:
            return "기본값";
        case DistributionRandomSeedMode::Manual:
            return "수동";
        default:
            return "기본값";
        }
    }

    const char* Get_AccelerationTimeBasisLabel(AccelerationTimeBasis basis)
    {
        switch (basis)
        {
        case AccelerationTimeBasis::ParticleLife:
            return "Particle Life";
        case AccelerationTimeBasis::EmitterNormalizedTime:
            return "Emitter Time";
        default:
            return "Particle Life";
        }
    }

    uint32 Generate_RandomSeedValue()
    {
        static uint32 state{ 0xA341316Cu };
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state != 0u ? state : 0x6D2B79F5u;
    }

    bool Draw_RandomSeedProperty(
        DetailPropertyContext& detailContext,
        DistributionRandomSeedData& data)
    {
        bool changed = false;

        if (!detailContext.Begin_PropertyTable("DistributionRandomSeed"))
            return false;

        ImGui::PushID("RandomSeedMode");
        detailContext.Draw_PropertyLabel("시드 모드", "같은 시드는 같은 랜덤 분포를 재현합니다.");
        DistributionRandomSeedMode nextMode = data.mode;
        if (ImGui::BeginCombo("##Value", Get_DistributionRandomSeedModeLabel(nextMode)))
        {
            const DistributionRandomSeedMode candidates[] = {
                DistributionRandomSeedMode::Default,
                DistributionRandomSeedMode::Manual,
            };

            for (const DistributionRandomSeedMode candidate : candidates)
            {
                const bool selected = nextMode == candidate;
                if (ImGui::Selectable(Get_DistributionRandomSeedModeLabel(candidate), selected))
                    nextMode = candidate;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (data.mode != nextMode)
        {
            data.mode = nextMode;
            changed = true;
        }
        ImGui::PopID();

        ImGui::PushID("ManualSeed");
        detailContext.Draw_PropertyLabel("시드");
        const float randomizeButtonWidth = ImGui::CalcTextSize("무작위").x + ImGui::GetStyle().FramePadding.x * 2.f;
        const float seedInputWidth = max(1.f, ImGui::GetContentRegionAvail().x - randomizeButtonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::SetNextItemWidth(seedInputWidth);
        const bool manualMode = data.mode == DistributionRandomSeedMode::Manual;
        if (!manualMode)
            ImGui::BeginDisabled();
        changed |= ImGui::InputScalar("##Value", ImGuiDataType_U32, &data.manualSeed);
        if (!manualMode)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("무작위"))
        {
            data.manualSeed = Generate_RandomSeedValue();
            data.mode = DistributionRandomSeedMode::Manual;
            changed = true;
        }
        ImGui::PopID();

        ImGui::PushID("UseInstanceSeed");
        detailContext.Draw_PropertyLabel(
            "재생마다 변주",
            "켜면 이 이펙트를 재생할 때마다 생성되는 seed를 이 랜덤값 샘플에 섞습니다."
        );
        changed |= ImGui::Checkbox("##Value", &data.useInstanceSeed);
        if (data.useInstanceSeed)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("켜짐");
        }
        ImGui::PopID();

        detailContext.End_PropertyTable();
        return changed;
    }

    const char* Get_SphereLocationSpawnModeLabel(SphereLocationSpawnMode mode)
    {
        switch (mode)
        {
        case SphereLocationSpawnMode::Volume:
            return "부피";
        case SphereLocationSpawnMode::Surface:
            return "표면";
        default:
            return "부피";
        }
    }

    const char* Get_SphereLocationPlacementModeLabel(SphereLocationPlacementMode mode)
    {
        switch (mode)
        {
        case SphereLocationPlacementMode::Random:
            return "랜덤 샘플";
        case SphereLocationPlacementMode::EvenByParticleIndex:
            return "인덱스 균등";
        default:
            return "랜덤 샘플";
        }
    }

    const char* Get_PlaneRadialLocationPlaneLabel(PlaneRadialLocationPlane plane)
    {
        switch (plane)
        {
        case PlaneRadialLocationPlane::XY:
            return "XY";
        case PlaneRadialLocationPlane::XZ:
            return "XZ";
        case PlaneRadialLocationPlane::YZ:
            return "YZ";
        case PlaneRadialLocationPlane::CameraFacing:
            return "카메라 정면";
        default:
            return "XY";
        }
    }

    const char* Get_PlaneRadialLocationShapeLabel(PlaneRadialLocationShape shape)
    {
        switch (shape)
        {
        case PlaneRadialLocationShape::Rectangle:
            return "사각";
        case PlaneRadialLocationShape::Disc:
            return "원반";
        case PlaneRadialLocationShape::Ring:
            return "링";
        case PlaneRadialLocationShape::Arc:
            return "호";
        default:
            return "원반";
        }
    }

    const char* Get_PlaneRadialLocationPlacementModeLabel(PlaneRadialLocationPlacementMode mode)
    {
        switch (mode)
        {
        case PlaneRadialLocationPlacementMode::Random:
            return "랜덤 샘플";
        case PlaneRadialLocationPlacementMode::EvenByParticleIndex:
            return "인덱스 균등";
        default:
            return "랜덤 샘플";
        }
    }

    const char* Get_CylinderLocationAxisLabel(CylinderLocationAxis axis)
    {
        switch (axis)
        {
        case CylinderLocationAxis::LocalX:
            return "Local X";
        case CylinderLocationAxis::LocalY:
            return "Local Y";
        case CylinderLocationAxis::LocalLook:
        default:
            return "Local Look";
        }
    }

    const char* Get_CylinderLocationSpawnModeLabel(CylinderLocationSpawnMode mode)
    {
        switch (mode)
        {
        case CylinderLocationSpawnMode::Volume:
            return "부피";
        case CylinderLocationSpawnMode::SideSurface:
        default:
            return "옆면";
        }
    }

    const char* Get_CylinderLocationPlacementModeLabel(CylinderLocationPlacementMode mode)
    {
        switch (mode)
        {
        case CylinderLocationPlacementMode::EvenByParticleIndex:
            return "인덱스 균등";
        case CylinderLocationPlacementMode::Random:
        default:
            return "랜덤 샘플";
        }
    }

    const char* Get_InitialRadialVelocityCenterDirectionModeLabel(InitialRadialVelocityCenterDirectionMode mode)
    {
        switch (mode)
        {
        case InitialRadialVelocityCenterDirectionMode::PlaneRadial:
            return "평면 방사";
        case InitialRadialVelocityCenterDirectionMode::RandomUpward:
        default:
            return "랜덤 상승";
        }
    }

    const char* Get_PlaneRadialOrientationTargetKindLabel(PlaneRadialOrientationTargetKind targetKind)
    {
        switch (targetKind)
        {
        case PlaneRadialOrientationTargetKind::Auto:
            return "자동";
        case PlaneRadialOrientationTargetKind::Sprite2D:
            return "스프라이트";
        case PlaneRadialOrientationTargetKind::Mesh3D:
            return "메시";
        default:
            return "자동";
        }
    }

    PlaneRadialOrientationTargetKind Resolve_PlaneRadialOrientationTargetKind(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::Mesh
               ? PlaneRadialOrientationTargetKind::Mesh3D
               : PlaneRadialOrientationTargetKind::Sprite2D;
    }

    bool Is_MeshEffectAuthoring(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    }

    bool Is_PlaneRadialLocationAdvancedPlacementShape(PlaneRadialLocationShape shape)
    {
        return shape != PlaneRadialLocationShape::Rectangle;
    }

    const char* Get_PlaneRadialOrientationModeLabel(PlaneRadialOrientationMode mode)
    {
        switch (mode)
        {
        case PlaneRadialOrientationMode::None:
            return "없음";
        case PlaneRadialOrientationMode::FaceRadialOut:
            return "바깥쪽";
        case PlaneRadialOrientationMode::FaceRadialIn:
            return "안쪽";
        case PlaneRadialOrientationMode::FaceTangentCW:
            return "접선 CW";
        case PlaneRadialOrientationMode::FaceTangentCCW:
            return "접선 CCW";
        case PlaneRadialOrientationMode::FacePlaneNormal:
            return "평면 normal";
        default:
            return "없음";
        }
    }

    const char* Get_CylinderOrientationModeLabel(CylinderOrientationMode mode)
    {
        switch (mode)
        {
        case CylinderOrientationMode::None:
            return "없음";
        case CylinderOrientationMode::FaceRadialOut:
            return "바깥쪽";
        case CylinderOrientationMode::FaceRadialIn:
            return "안쪽";
        case CylinderOrientationMode::FaceTangentCW:
            return "접선 CW";
        case CylinderOrientationMode::FaceTangentCCW:
            return "접선 CCW";
        case CylinderOrientationMode::FaceCylinderAxisPositive:
            return "축 +";
        case CylinderOrientationMode::FaceCylinderAxisNegative:
            return "축 -";
        default:
            return "없음";
        }
    }

    const char* Get_SphereRadialOrientationModeLabel(SphereRadialOrientationMode mode)
    {
        switch (mode)
        {
        case SphereRadialOrientationMode::None:
            return "없음";
        case SphereRadialOrientationMode::FaceRadialOut:
            return "바깥쪽";
        case SphereRadialOrientationMode::FaceRadialIn:
            return "안쪽";
        default:
            return "없음";
        }
    }

    const char* Get_EffectOrbitPlaneLabel(EffectOrbitPlane plane)
    {
        switch (plane)
        {
        case EffectOrbitPlane::XY:
            return "XY";
        case EffectOrbitPlane::XZ:
            return "XZ";
        case EffectOrbitPlane::YZ:
            return "YZ";
        default:
            return "XY";
        }
    }

    const char* Get_PlaneRadialOrientationAxisLabel(PlaneRadialOrientationAxis axis)
    {
        switch (axis)
        {
        case PlaneRadialOrientationAxis::PositiveX:
            return "+X";
        case PlaneRadialOrientationAxis::NegativeX:
            return "-X";
        case PlaneRadialOrientationAxis::PositiveY:
            return "+Y";
        case PlaneRadialOrientationAxis::NegativeY:
            return "-Y";
        case PlaneRadialOrientationAxis::PositiveZ:
            return "+Z";
        case PlaneRadialOrientationAxis::NegativeZ:
            return "-Z";
        default:
            return "+Z";
        }
    }

    const char* Get_MeshDirectionAlignTargetModeLabel(MeshDirectionAlignTargetMode mode)
    {
        switch (mode)
        {
        case MeshDirectionAlignTargetMode::Point:
            return "Point";
        case MeshDirectionAlignTargetMode::Direction:
        default:
            return "Direction";
        }
    }

    const char* Get_MeshDirectionAlignSpaceLabel(MeshDirectionAlignSpace space)
    {
        switch (space)
        {
        case MeshDirectionAlignSpace::Local:
            return "Local";
        case MeshDirectionAlignSpace::World:
        default:
            return "World";
        }
    }

    const char* Get_MeshDirectionAlignBlendModeLabel(MeshDirectionAlignBlendMode mode)
    {
        switch (mode)
        {
        case MeshDirectionAlignBlendMode::EaseIn:
            return "Ease In";
        case MeshDirectionAlignBlendMode::EaseOut:
            return "Ease Out";
        case MeshDirectionAlignBlendMode::EaseInOut:
            return "Ease InOut";
        case MeshDirectionAlignBlendMode::Linear:
        default:
            return "Linear";
        }
    }

    const char* Get_MaterialScalarTargetLabel(MaterialScalarModulationTargetField target)
    {
        switch (target)
        {
        case MaterialScalarModulationTargetField::Intensity:
            return "Intensity";
        case MaterialScalarModulationTargetField::OpacityPower:
            return "OpacityPower";
        case MaterialScalarModulationTargetField::NoiseStrength:
            return "NoiseStrength";
        case MaterialScalarModulationTargetField::AlphaErosion:
            return "AlphaErosion";
        case MaterialScalarModulationTargetField::AlphaCutoff:
            return "AlphaCutoff";
        case MaterialScalarModulationTargetField::AlphaMultiplier:
            return "AlphaMultiplier";
        case MaterialScalarModulationTargetField::CoreIntensity:
            return "CoreIntensity";
        case MaterialScalarModulationTargetField::OuterIntensity:
            return "OuterIntensity";
        case MaterialScalarModulationTargetField::CoreColor:
            return "CoreColor Multiplier";
        case MaterialScalarModulationTargetField::RefractionIntensity:
            return "Refraction Intensity";
        case MaterialScalarModulationTargetField::MainUVScrollSpeedScale:
            return "Main UV Scroll Speed Scale";
        case MaterialScalarModulationTargetField::NoiseUVScrollSpeedScale:
            return "Noise UV Scroll Speed Scale";
        case MaterialScalarModulationTargetField::MaskUVScrollSpeedScale:
            return "Mask UV Scroll Speed Scale";
        case MaterialScalarModulationTargetField::FlowUVScrollSpeedScale:
            return "Flow UV Scroll Speed Scale";
        default:
            return "Intensity";
        }
    }

    const char* Get_MaterialVec2TargetLabel(MaterialVec2ModulationTargetField target)
    {
        switch (target)
        {
        case MaterialVec2ModulationTargetField::NoiseUVOffset:
            return "Noise UV Offset";
        case MaterialVec2ModulationTargetField::MaskUVOffset:
            return "Mask UV Offset";
        case MaterialVec2ModulationTargetField::FlowUVOffset:
            return "Flow UV Offset";
        case MaterialVec2ModulationTargetField::MainUVOffset:
        default:
            return "Main UV Offset";
        }
    }

    enum class MaterialModulationTargetCategory : uint8
    {
        Scalar,
        CoreColorRgb,
        Vec2,
    };

    struct MaterialModulationTargetSelection
    {
        MaterialModulationTargetCategory category{ MaterialModulationTargetCategory::Scalar };
        MaterialScalarModulationTargetField scalarTarget{ MaterialScalarModulationTargetField::Intensity };
        MaterialVec2ModulationTargetField vec2Target{ MaterialVec2ModulationTargetField::MainUVOffset };
    };

    bool Is_MaterialScrollSpeedScaleTarget(MaterialScalarModulationTargetField target)
    {
        return target == MaterialScalarModulationTargetField::MainUVScrollSpeedScale ||
               target == MaterialScalarModulationTargetField::NoiseUVScrollSpeedScale ||
               target == MaterialScalarModulationTargetField::MaskUVScrollSpeedScale ||
               target == MaterialScalarModulationTargetField::FlowUVScrollSpeedScale;
    }

    const char* Get_MaterialModulationTargetLabel(const MaterialModulationTargetSelection& selection)
    {
        if (selection.category == MaterialModulationTargetCategory::CoreColorRgb)
            return "Core Color RGB";
        if (selection.category == MaterialModulationTargetCategory::Vec2)
            return Get_MaterialVec2TargetLabel(selection.vec2Target);

        return Get_MaterialScalarTargetLabel(selection.scalarTarget);
    }

    const char* Get_MaterialModulationTargetLabel(bool isCoreColorRgb, MaterialScalarModulationTargetField target)
    {
        return isCoreColorRgb ? "Core Color RGB" : Get_MaterialScalarTargetLabel(target);
    }

    const char* Get_MaterialScalarOperationLabel(MaterialScalarModulationOperation operation)
    {
        switch (operation)
        {
        case MaterialScalarModulationOperation::Multiply:
            return "Multiply";
        default:
            return "Multiply";
        }
    }

    const char* Get_MaterialScalarTimeSourceLabel(MaterialScalarModulationTimeSource timeSource)
    {
        switch (timeSource)
        {
        case MaterialScalarModulationTimeSource::ParticleLife:
            return "ParticleLife";
        case MaterialScalarModulationTimeSource::EmitterTime:
            return "EmitterTime";
        default:
            return "EmitterTime";
        }
    }

    bool Is_MaterialScalarParticleLifeSupported(const AuthoringEmitter& emitter)
    {
        return emitter.rendererType == "Sprite" ||
               emitter.rendererType == "Trail" ||
               emitter.rendererType == "Ribbon" ||
               emitter.rendererType == "Beam" ||
               emitter.rendererType == "Mesh" ||
               emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail ||
               emitter.typeData.kind == AuthoringTypeDataKind::Trail ||
               emitter.typeData.kind == AuthoringTypeDataKind::Ribbon ||
               emitter.typeData.kind == AuthoringTypeDataKind::Beam ||
               emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    }

    bool Is_MaterialScalarTimeSourceSelectable(
        MaterialScalarModulationTimeSource timeSource,
        bool allowParticleLife)
    {
        return timeSource != MaterialScalarModulationTimeSource::ParticleLife || allowParticleLife;
    }

    bool Draw_MaterialModulationTargetProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        const MaterialModulationTargetSelection& current,
        MaterialModulationTargetSelection& outNext)
    {
        bool changed = false;
        outNext = current;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "변조할 material field입니다. Scroll Speed Scale은 base UV scroll speed에 scalar 값을 곱하고, UV Offset은 Vec2 값을 더합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MaterialModulationTargetLabel(current)))
        {
            ImGui::TextDisabled("Scalar");
            const MaterialScalarModulationTargetField candidates[] = {
                MaterialScalarModulationTargetField::Intensity,
                MaterialScalarModulationTargetField::OpacityPower,
                MaterialScalarModulationTargetField::NoiseStrength,
                MaterialScalarModulationTargetField::AlphaErosion,
                MaterialScalarModulationTargetField::AlphaCutoff,
                MaterialScalarModulationTargetField::AlphaMultiplier,
                MaterialScalarModulationTargetField::CoreIntensity,
                MaterialScalarModulationTargetField::OuterIntensity,
                MaterialScalarModulationTargetField::CoreColor,
                MaterialScalarModulationTargetField::RefractionIntensity,
                MaterialScalarModulationTargetField::MainUVScrollSpeedScale,
                MaterialScalarModulationTargetField::NoiseUVScrollSpeedScale,
                MaterialScalarModulationTargetField::MaskUVScrollSpeedScale,
                MaterialScalarModulationTargetField::FlowUVScrollSpeedScale,
            };

            for (const MaterialScalarModulationTargetField candidate : candidates)
            {
                const bool isSelected = current.category == MaterialModulationTargetCategory::Scalar && current.scalarTarget == candidate;
                if (ImGui::Selectable(Get_MaterialScalarTargetLabel(candidate), isSelected))
                {
                    outNext.category = MaterialModulationTargetCategory::Scalar;
                    outNext.scalarTarget = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();
            ImGui::TextDisabled("Color");
            const bool isRgbSelected = current.category == MaterialModulationTargetCategory::CoreColorRgb;
            if (ImGui::Selectable("Core Color RGB", isRgbSelected))
            {
                outNext.category = MaterialModulationTargetCategory::CoreColorRgb;
                outNext.scalarTarget = MaterialScalarModulationTargetField::CoreColor;
                changed = true;
            }

            if (isRgbSelected)
                ImGui::SetItemDefaultFocus();

            ImGui::Separator();
            ImGui::TextDisabled("Vec2");
            const MaterialVec2ModulationTargetField vec2Candidates[] = {
                MaterialVec2ModulationTargetField::MainUVOffset,
                MaterialVec2ModulationTargetField::NoiseUVOffset,
                MaterialVec2ModulationTargetField::MaskUVOffset,
                MaterialVec2ModulationTargetField::FlowUVOffset,
            };

            for (const MaterialVec2ModulationTargetField candidate : vec2Candidates)
            {
                const bool isSelected = current.category == MaterialModulationTargetCategory::Vec2 && current.vec2Target == candidate;
                if (ImGui::Selectable(Get_MaterialVec2TargetLabel(candidate), isSelected))
                {
                    outNext.category = MaterialModulationTargetCategory::Vec2;
                    outNext.vec2Target = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool canReset =
            current.category != MaterialModulationTargetCategory::Scalar ||
            current.scalarTarget != MaterialScalarModulationTargetField::Intensity;
        if (detailContext.Draw_ResetButton(canReset))
        {
            outNext = MaterialModulationTargetSelection{};
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_MaterialScalarTimeSourceProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        MaterialScalarModulationTimeSource& value,
        MaterialScalarModulationTimeSource defaultValue,
        bool allowParticleLife)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "ParticleLife는 개별 particle 수명 진행도, EmitterTime은 emitter 재생 시간을 기준으로 분포를 평가합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MaterialScalarTimeSourceLabel(value)))
        {
            const MaterialScalarModulationTimeSource candidates[] = {
                MaterialScalarModulationTimeSource::ParticleLife,
                MaterialScalarModulationTimeSource::EmitterTime,
            };

            for (const MaterialScalarModulationTimeSource candidate : candidates)
            {
                const bool isSelected = value == candidate;
                const bool selectable = Is_MaterialScalarTimeSourceSelectable(candidate, allowParticleLife);
                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_MaterialScalarTimeSourceLabel(candidate), isSelected) && selectable)
                {
                    value = candidate;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        if (!allowParticleLife && value == MaterialScalarModulationTimeSource::ParticleLife)
            ImGui::TextDisabled("ParticleLife is not supported for this emitter type or row.");

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_SphereLocationSpawnModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereLocationSpawnMode& value,
        SphereLocationSpawnMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "Volume은 구 내부, Surface는 구 표면에서 spawn 위치를 샘플합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SphereLocationSpawnModeLabel(value)))
        {
            const SphereLocationSpawnMode candidates[] = {
                SphereLocationSpawnMode::Volume,
                SphereLocationSpawnMode::Surface,
            };

            for (const SphereLocationSpawnMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereLocationSpawnModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_SphereLocationPlacementModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereLocationPlacementMode& value,
        SphereLocationPlacementMode defaultValue,
        bool enabled)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "Surface 생성에서 표면 위치를 랜덤으로 뽑을지 particle index 기준으로 고르게 배치할지 정합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!enabled)
            ImGui::BeginDisabled();

        if (ImGui::BeginCombo("##Value", Get_SphereLocationPlacementModeLabel(value)))
        {
            const SphereLocationPlacementMode candidates[] = {
                SphereLocationPlacementMode::Random,
                SphereLocationPlacementMode::EvenByParticleIndex,
            };

            for (const SphereLocationPlacementMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereLocationPlacementModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (!enabled)
            ImGui::EndDisabled();

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_PlaneRadialLocationPlaneProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialLocationPlane& value,
        PlaneRadialLocationPlane defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialLocationPlaneLabel(value)))
        {
            const PlaneRadialLocationPlane candidates[] = {
                PlaneRadialLocationPlane::XY,
                PlaneRadialLocationPlane::XZ,
                PlaneRadialLocationPlane::YZ,
                PlaneRadialLocationPlane::CameraFacing,
            };

            for (const PlaneRadialLocationPlane candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialLocationPlaneLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_PlaneRadialLocationShapeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialLocationShape& value,
        PlaneRadialLocationShape defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialLocationShapeLabel(value)))
        {
            const PlaneRadialLocationShape candidates[] = {
                PlaneRadialLocationShape::Rectangle,
                PlaneRadialLocationShape::Disc,
                PlaneRadialLocationShape::Ring,
                PlaneRadialLocationShape::Arc,
            };

            for (const PlaneRadialLocationShape candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialLocationShapeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_PlaneRadialLocationPlacementModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialLocationPlacementMode& value,
        PlaneRadialLocationPlacementMode defaultValue,
        bool enabled)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!enabled)
            ImGui::BeginDisabled();

        if (ImGui::BeginCombo("##Value", Get_PlaneRadialLocationPlacementModeLabel(value)))
        {
            const PlaneRadialLocationPlacementMode candidates[] = {
                PlaneRadialLocationPlacementMode::Random,
                PlaneRadialLocationPlacementMode::EvenByParticleIndex,
            };

            for (const PlaneRadialLocationPlacementMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialLocationPlacementModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (!enabled)
            ImGui::EndDisabled();

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_CylinderLocationAxisProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        CylinderLocationAxis& value,
        CylinderLocationAxis defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "원통 길이축으로 사용할 emitter local axis입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_CylinderLocationAxisLabel(value)))
        {
            const CylinderLocationAxis candidates[] = {
                CylinderLocationAxis::LocalX,
                CylinderLocationAxis::LocalY,
                CylinderLocationAxis::LocalLook,
            };

            for (const CylinderLocationAxis candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_CylinderLocationAxisLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_CylinderLocationSpawnModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        CylinderLocationSpawnMode& value,
        CylinderLocationSpawnMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "옆면은 원통 표면/shell, 부피는 원통 내부 공간에서 spawn 위치를 샘플합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_CylinderLocationSpawnModeLabel(value)))
        {
            const CylinderLocationSpawnMode candidates[] = {
                CylinderLocationSpawnMode::SideSurface,
                CylinderLocationSpawnMode::Volume,
            };

            for (const CylinderLocationSpawnMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_CylinderLocationSpawnModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_CylinderLocationPlacementModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        CylinderLocationPlacementMode& value,
        CylinderLocationPlacementMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, "원주 angle을 랜덤으로 뽑을지 particle index 기준으로 고르게 배치할지 정합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_CylinderLocationPlacementModeLabel(value)))
        {
            const CylinderLocationPlacementMode candidates[] = {
                CylinderLocationPlacementMode::Random,
                CylinderLocationPlacementMode::EvenByParticleIndex,
            };

            for (const CylinderLocationPlacementMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_CylinderLocationPlacementModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_InitialRadialVelocityCenterDirectionModeProperty(
        DetailPropertyContext& detailContext,
        InitialRadialVelocityCenterDirectionMode& value,
        InitialRadialVelocityCenterDirectionMode defaultValue)
    {
        bool changed = false;
        ImGui::PushID("InitialRadialVelocityCenterDirectionMode");
        detailContext.Draw_PropertyLabel(
            "방향 없음 처리",
            "생성 위치와 방사 기준점이 같아 방사 방향을 계산할 수 없을 때 사용할 방향입니다."
        );

        if (ImGui::BeginCombo("##Value", Get_InitialRadialVelocityCenterDirectionModeLabel(value)))
        {
            const InitialRadialVelocityCenterDirectionMode candidates[] = {
                InitialRadialVelocityCenterDirectionMode::RandomUpward,
                InitialRadialVelocityCenterDirectionMode::PlaneRadial,
            };

            for (const InitialRadialVelocityCenterDirectionMode candidate : candidates)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_InitialRadialVelocityCenterDirectionModeLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_AccelerationTimeBasisProperty(
        DetailPropertyContext& detailContext,
        AccelerationTimeBasis& value,
        AccelerationTimeBasis defaultValue)
    {
        bool changed = false;

        ImGui::PushID("AccelerationTimeBasis");
        detailContext.Draw_PropertyLabel(
            "Time Basis",
            "Particle Life는 각 particle 생애 기준, Emitter Time은 emitter loop 진행률 기준으로 가속 curve를 평가합니다."
        );
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_AccelerationTimeBasisLabel(value)))
        {
            const AccelerationTimeBasis candidates[] = {
                AccelerationTimeBasis::ParticleLife,
                AccelerationTimeBasis::EmitterNormalizedTime,
            };

            for (const AccelerationTimeBasis candidate : candidates)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_AccelerationTimeBasisLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    const char* Get_VelocityOverLifeApplyChannelLabel(VelocityOverLifeApplyChannel channel)
    {
        switch (channel)
        {
        case VelocityOverLifeApplyChannel::InitialVelocity:
            return "Initial Velocity";
        case VelocityOverLifeApplyChannel::InitialRadialVelocity:
            return "Initial Radial Velocity";
        case VelocityOverLifeApplyChannel::VelocityCone:
            return "Velocity Cone";
        case VelocityOverLifeApplyChannel::SourceMotionVelocity:
            return "Source Motion Velocity";
        case VelocityOverLifeApplyChannel::AccelerationIntegratedVelocity:
            return "Acceleration Integrated Velocity";
        default:
            return "Initial Velocity";
        }
    }

    uint32 To_VelocityOverLifeApplyChannelMask(VelocityOverLifeApplyChannel channel)
    {
        switch (channel)
        {
        case VelocityOverLifeApplyChannel::InitialVelocity:
            return kVelocityOverLifeApplyChannelInitialVelocityMask;
        case VelocityOverLifeApplyChannel::InitialRadialVelocity:
            return kVelocityOverLifeApplyChannelInitialRadialVelocityMask;
        case VelocityOverLifeApplyChannel::VelocityCone:
            return kVelocityOverLifeApplyChannelVelocityConeMask;
        case VelocityOverLifeApplyChannel::SourceMotionVelocity:
            return kVelocityOverLifeApplyChannelSourceMotionVelocityMask;
        case VelocityOverLifeApplyChannel::AccelerationIntegratedVelocity:
            return kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask;
        default:
            return 0u;
        }
    }

    bool Has_VelocityOverLifeChannelConflict(
        const AuthoringEmitter& emitter,
        const AuthoringModule& currentModule,
        const VelocityOverLifeModuleData& currentData)
    {
        if (!currentModule.enabled)
            return false;

        const uint32 currentMask = currentData.applyChannelMask != 0u
                                   ? currentData.applyChannelMask
                                   : kVelocityOverLifeApplyChannelDefaultMask;
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.id == currentModule.id || module.type != AuthoringModuleType::VelocityOverLife || !module.enabled)
                continue;

            const auto* data = get_if<VelocityOverLifeModuleData>(&module.data);
            if (data == nullptr)
                continue;

            const uint32 otherMask = data->applyChannelMask != 0u
                                     ? data->applyChannelMask
                                     : kVelocityOverLifeApplyChannelDefaultMask;
            if ((currentMask & otherMask) != 0u)
                return true;
        }

        return false;
    }

    bool Draw_VelocityOverLifeApplyChannelMaskProperty(
        DetailPropertyContext& detailContext,
        uint32& mask,
        uint32 defaultMask)
    {
        bool changed = false;
        if (mask == 0u)
            mask = kVelocityOverLifeApplyChannelDefaultMask;

        ImGui::PushID("VelocityOverLifeApplyChannels");
        detailContext.Draw_PropertyLabel(
            "적용 대상",
            "VelocityOverLife 배율을 적용할 velocity channel입니다. 겹친 channel은 모듈 목록 위쪽 항목이 우선합니다."
        );

        const VelocityOverLifeApplyChannel channels[] = {
            VelocityOverLifeApplyChannel::InitialVelocity,
            VelocityOverLifeApplyChannel::InitialRadialVelocity,
            VelocityOverLifeApplyChannel::VelocityCone,
            VelocityOverLifeApplyChannel::SourceMotionVelocity,
            VelocityOverLifeApplyChannel::AccelerationIntegratedVelocity,
        };
        for (const VelocityOverLifeApplyChannel channel : channels)
        {
            const uint32 channelMask = To_VelocityOverLifeApplyChannelMask(channel);
            bool enabled = (mask & channelMask) != 0u;
            if (ImGui::Checkbox(Get_VelocityOverLifeApplyChannelLabel(channel), &enabled))
            {
                const uint32 nextMask = enabled ? mask | channelMask : mask & ~channelMask;
                if (nextMask != 0u)
                {
                    mask = nextMask;
                    changed = true;
                }
            }
        }

        if (detailContext.Draw_ResetButton(mask != defaultMask))
        {
            mask = defaultMask;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_PlaneRadialOrientationModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialOrientationMode& value,
        PlaneRadialOrientationMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialOrientationModeLabel(value)))
        {
            const PlaneRadialOrientationMode candidates[] = {
                PlaneRadialOrientationMode::None,
                PlaneRadialOrientationMode::FaceRadialOut,
                PlaneRadialOrientationMode::FaceRadialIn,
                PlaneRadialOrientationMode::FaceTangentCW,
                PlaneRadialOrientationMode::FaceTangentCCW,
                PlaneRadialOrientationMode::FacePlaneNormal,
            };

            for (const PlaneRadialOrientationMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialOrientationModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_SphereRadialOrientationModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereRadialOrientationMode& value,
        SphereRadialOrientationMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SphereRadialOrientationModeLabel(value)))
        {
            const SphereRadialOrientationMode candidates[] = {
                SphereRadialOrientationMode::None,
                SphereRadialOrientationMode::FaceRadialOut,
                SphereRadialOrientationMode::FaceRadialIn,
            };

            for (const SphereRadialOrientationMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereRadialOrientationModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_CylinderOrientationModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        CylinderOrientationMode& value,
        CylinderOrientationMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_CylinderOrientationModeLabel(value)))
        {
            const CylinderOrientationMode candidates[] = {
                CylinderOrientationMode::None,
                CylinderOrientationMode::FaceRadialOut,
                CylinderOrientationMode::FaceRadialIn,
                CylinderOrientationMode::FaceTangentCW,
                CylinderOrientationMode::FaceTangentCCW,
                CylinderOrientationMode::FaceCylinderAxisPositive,
                CylinderOrientationMode::FaceCylinderAxisNegative,
            };

            for (const CylinderOrientationMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_CylinderOrientationModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_EffectOrbitPlaneProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EffectOrbitPlane& value,
        EffectOrbitPlane defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_EffectOrbitPlaneLabel(value)))
        {
            const EffectOrbitPlane candidates[] = {
                EffectOrbitPlane::XY,
                EffectOrbitPlane::XZ,
                EffectOrbitPlane::YZ,
            };

            for (const EffectOrbitPlane candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_EffectOrbitPlaneLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    void Draw_ReadOnlyTextProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        const char* value,
        const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(value);
        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
    }

    void Draw_PlaneRadialOrientationResolvedTargetProperty(
        DetailPropertyContext& detailContext,
        const AuthoringEmitter& emitter)
    {
        Draw_ReadOnlyTextProperty(
            detailContext,
            "대상",
            Get_PlaneRadialOrientationTargetKindLabel(Resolve_PlaneRadialOrientationTargetKind(emitter)),
            "현재 emitter 타입 기준으로 자동 결정됩니다. targetKind 저장값은 유지하지만 이번 단계에서는 편집 UI를 숨깁니다."
        );
    }

    bool Draw_PlaneRadialOrientationAxisProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialOrientationAxis& value,
        PlaneRadialOrientationAxis defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialOrientationAxisLabel(value)))
        {
            const PlaneRadialOrientationAxis candidates[] = {
                PlaneRadialOrientationAxis::PositiveX,
                PlaneRadialOrientationAxis::NegativeX,
                PlaneRadialOrientationAxis::PositiveY,
                PlaneRadialOrientationAxis::NegativeY,
                PlaneRadialOrientationAxis::PositiveZ,
                PlaneRadialOrientationAxis::NegativeZ,
            };

            for (const PlaneRadialOrientationAxis candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialOrientationAxisLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_MeshDirectionAlignTargetModeProperty(
        DetailPropertyContext& detailContext,
        MeshDirectionAlignTargetMode& value,
        MeshDirectionAlignTargetMode defaultValue)
    {
        static constexpr MeshDirectionAlignTargetMode candidates[] = {
            MeshDirectionAlignTargetMode::Direction,
            MeshDirectionAlignTargetMode::Point,
        };

        bool changed = false;
        ImGui::PushID("MeshDirectionAlignTargetMode");
        detailContext.Draw_PropertyLabel("대상 방식");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MeshDirectionAlignTargetModeLabel(value)))
        {
            for (const MeshDirectionAlignTargetMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_MeshDirectionAlignTargetModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_MeshDirectionAlignSpaceProperty(
        DetailPropertyContext& detailContext,
        MeshDirectionAlignSpace& value,
        MeshDirectionAlignSpace defaultValue)
    {
        static constexpr MeshDirectionAlignSpace candidates[] = {
            MeshDirectionAlignSpace::Local,
            MeshDirectionAlignSpace::World,
        };

        bool changed = false;
        ImGui::PushID("MeshDirectionAlignSpace");
        detailContext.Draw_PropertyLabel("공간");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MeshDirectionAlignSpaceLabel(value)))
        {
            for (const MeshDirectionAlignSpace candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_MeshDirectionAlignSpaceLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_MeshDirectionAlignBlendModeProperty(
        DetailPropertyContext& detailContext,
        MeshDirectionAlignBlendMode& value,
        MeshDirectionAlignBlendMode defaultValue)
    {
        static constexpr MeshDirectionAlignBlendMode candidates[] = {
            MeshDirectionAlignBlendMode::Linear,
            MeshDirectionAlignBlendMode::EaseIn,
            MeshDirectionAlignBlendMode::EaseOut,
            MeshDirectionAlignBlendMode::EaseInOut,
        };

        bool changed = false;
        ImGui::PushID("MeshDirectionAlignBlendMode");
        detailContext.Draw_PropertyLabel("보간 방식", "정렬 진행도 weight를 보정하는 방식입니다. 회전 속도나 물리 steering 값이 아닙니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MeshDirectionAlignBlendModeLabel(value)))
        {
            for (const MeshDirectionAlignBlendMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_MeshDirectionAlignBlendModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Has_PlaneRadialLocationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type == AuthoringModuleType::PlaneRadialLocation)
                return true;
        }

        return false;
    }

    bool Has_CylinderLocationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type == AuthoringModuleType::CylinderLocation)
                return true;
        }

        return false;
    }

    bool Has_SphereLocationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type == AuthoringModuleType::SphereLocation)
                return true;
        }

        return false;
    }

    bool Has_EnabledModule(const AuthoringEmitter& emitter, AuthoringModuleType type)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type == type && module.enabled)
                return true;
        }

        return false;
    }

    bool Has_EnabledPlaneRadialOrientationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::PlaneRadialOrientation || !module.enabled)
                continue;

            const auto* data = get_if<PlaneRadialOrientationModuleData>(&module.data);
            if (data != nullptr && data->orientationMode != PlaneRadialOrientationMode::None)
                return true;
        }

        return false;
    }

    bool Should_ShowPlaneRadialOrientationMeshFields(const AuthoringEmitter& emitter)
    {
        return Resolve_PlaneRadialOrientationTargetKind(emitter) == PlaneRadialOrientationTargetKind::Mesh3D;
    }

    bool Draw_SubUVFramePlaybackModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SubUVFramePlaybackMode& value,
        SubUVFramePlaybackMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SubUVFramePlaybackModeLabel(value)))
        {
            const SubUVFramePlaybackMode candidates[] = {
                SubUVFramePlaybackMode::FixedFrame,
                SubUVFramePlaybackMode::LifeProgress,
                SubUVFramePlaybackMode::FramesPerSecond,
                SubUVFramePlaybackMode::RandomFrame,
            };

            for (const SubUVFramePlaybackMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SubUVFramePlaybackModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Normalize_SubUVFrameIndexDistribution(FloatDistributionData& data, uint32 lastFrame)
    {
        bool changed = false;
        const float minFrame = 0.f;
        const float maxFrame = static_cast<float>(lastFrame);

        if (data.mode == DistributionMode::Uniform)
        {
            data = FloatDistributionData::Make_ConstantCurve(0.f);
            return true;
        }

        if (auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
        {
            const float clampedValue = clamp(constant->value, minFrame, maxFrame);
            if (!NearlyEqual(constant->value, clampedValue))
            {
                constant->value = clampedValue;
                changed = true;
            }
        }
        else if (auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
        {
            for (FloatCurveKeyData& key : curve->keys)
            {
                const float clampedValue = clamp(key.value, minFrame, maxFrame);
                if (!NearlyEqual(key.value, clampedValue))
                {
                    key.value = clampedValue;
                    changed = true;
                }
            }
        }

        return changed;
    }

    bool Is_SameMaterial(const EffectMaterialInstanceData& lhs, const EffectMaterialInstanceData& rhs)
    {
        return
            lhs.mainTextureGuid == rhs.mainTextureGuid &&
            lhs.mainTexturePath == rhs.mainTexturePath &&
            lhs.noiseTextureGuid == rhs.noiseTextureGuid &&
            lhs.noiseTexturePath == rhs.noiseTexturePath &&
            lhs.maskTextureGuid == rhs.maskTextureGuid &&
            lhs.maskTexturePath == rhs.maskTexturePath &&
            Is_SameColor(lhs.tint, rhs.tint) &&
            NearlyEqual(lhs.intensity, rhs.intensity) &&
            NearlyEqual(lhs.opacityPower, rhs.opacityPower) &&
            NearlyEqual(lhs.alphaMultiplier, rhs.alphaMultiplier) &&
            NearlyEqual(lhs.noiseStrength, rhs.noiseStrength) &&
            NearlyEqual(lhs.alphaCutoff, rhs.alphaCutoff) &&
            NearlyEqual(lhs.alphaErosion, rhs.alphaErosion) &&
            lhs.noiseSource == rhs.noiseSource &&
            lhs.maskSource == rhs.maskSource &&
            lhs.noiseInvert == rhs.noiseInvert &&
            lhs.maskInvert == rhs.maskInvert &&
            Is_SameVec2(lhs.mainUVScale, rhs.mainUVScale) &&
            Is_SameVec2(lhs.mainUVOffset, rhs.mainUVOffset) &&
            Is_SameVec2(lhs.mainUVScrollSpeed, rhs.mainUVScrollSpeed) &&
            lhs.mainUVTilingMode == rhs.mainUVTilingMode &&
            lhs.mainUVPolicy.uPolicy == rhs.mainUVPolicy.uPolicy &&
            lhs.mainUVPolicy.vPolicy == rhs.mainUVPolicy.vPolicy &&
            lhs.mainUVRotation == rhs.mainUVRotation &&
            Is_SameVec2(lhs.noiseUVScale, rhs.noiseUVScale) &&
            Is_SameVec2(lhs.noiseUVOffset, rhs.noiseUVOffset) &&
            Is_SameVec2(lhs.noiseUVScrollSpeed, rhs.noiseUVScrollSpeed) &&
            lhs.noiseUVTilingMode == rhs.noiseUVTilingMode &&
            lhs.noiseUVPolicy.uPolicy == rhs.noiseUVPolicy.uPolicy &&
            lhs.noiseUVPolicy.vPolicy == rhs.noiseUVPolicy.vPolicy &&
            lhs.noiseUVRotation == rhs.noiseUVRotation &&
            Is_SameVec2(lhs.maskUVScale, rhs.maskUVScale) &&
            Is_SameVec2(lhs.maskUVOffset, rhs.maskUVOffset) &&
            Is_SameVec2(lhs.maskUVScrollSpeed, rhs.maskUVScrollSpeed) &&
            lhs.maskUVTilingMode == rhs.maskUVTilingMode &&
            lhs.maskUVPolicy.uPolicy == rhs.maskUVPolicy.uPolicy &&
            lhs.maskUVPolicy.vPolicy == rhs.maskUVPolicy.vPolicy &&
            lhs.maskUVRotation == rhs.maskUVRotation &&
            Is_SameVec2(lhs.flowUVScale, rhs.flowUVScale) &&
            Is_SameVec2(lhs.flowUVOffset, rhs.flowUVOffset) &&
            Is_SameVec2(lhs.flowUVScrollSpeed, rhs.flowUVScrollSpeed) &&
            lhs.flowUVTilingMode == rhs.flowUVTilingMode &&
            lhs.flowUVPolicy.uPolicy == rhs.flowUVPolicy.uPolicy &&
            lhs.flowUVPolicy.vPolicy == rhs.flowUVPolicy.vPolicy &&
            lhs.flowUVRotation == rhs.flowUVRotation &&
            lhs.blendMode == rhs.blendMode &&
            lhs.opacitySource == rhs.opacitySource &&
            Is_SameAdditiveContribution(lhs.additive, rhs.additive) &&
            lhs.coreEmissive.enabled == rhs.coreEmissive.enabled &&
            Is_SameColor(lhs.coreEmissive.coreColor, rhs.coreEmissive.coreColor) &&
            NearlyEqual(lhs.coreEmissive.corePower, rhs.coreEmissive.corePower) &&
            NearlyEqual(lhs.coreEmissive.coreIntensity, rhs.coreEmissive.coreIntensity) &&
            NearlyEqual(lhs.coreEmissive.outerPower, rhs.coreEmissive.outerPower) &&
            NearlyEqual(lhs.coreEmissive.outerIntensity, rhs.coreEmissive.outerIntensity) &&
            lhs.twoSided == rhs.twoSided &&
            lhs.subUVRows == rhs.subUVRows &&
            lhs.subUVCols == rhs.subUVCols;
    }

    string Try_ResolvePresetNameFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!EffectMaterialPresetReader::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetMaterial.sourcePresetName;
        }

        return {};
    }

    wstring Try_ResolvePresetPathFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        if (!material.sourcePresetName.empty())
        {
            const fs::path candidatePath =
                presetFolder / (String::ToWString(material.sourcePresetName) + L".effectmaterial.json");
            if (fs::exists(candidatePath) && fs::is_regular_file(candidatePath))
                return candidatePath.wstring();
        }

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!EffectMaterialPresetReader::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetPath.wstring();
        }

        return {};
    }

    const string& Resolve_MaterialDisplayName(RequiredModuleData& data)
    {
        if (data.material.sourcePresetName.empty())
            data.material.sourcePresetName = Try_ResolvePresetNameFromMaterial(data.material);

        return data.material.sourcePresetName;
    }

    bool Try_PickMaterialPresetDialog(wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUnInit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUnInit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(L"Select Effect Material");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!defaultFolder.empty())
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"Effect Materials", L"*.effectmaterial.json" },
            { L"All", L"*.*" },
        };

        dialog->SetFileTypes(std::size(filters), filters);

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUnInit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    Shared<EffectMaterial_View> Get_MaterialView()
    {
        if (nullptr == EDITOR)
            return nullptr;

        const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
        return dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    }

    void Open_MaterialInstanceWindow(uint32 emitterId, uint32 moduleId)
    {
        if (nullptr == EDITOR)
            return;

        const Shared<Editor_Window> materialInstanceWindow = EDITOR->Get_Window(L"Effect Material Instance");
        const Shared<EffectMaterialInstance_View> materialInstanceView =
            dynamic_pointer_cast<EffectMaterialInstance_View>(materialInstanceWindow);
        if (nullptr == materialInstanceView)
            return;

        materialInstanceView->Open_Instance(emitterId, moduleId);
    }

    void Open_PresetBrowserWindow(const RequiredModuleData& data)
    {
        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (nullptr == materialView)
            return;

        const wstring presetPath = Try_ResolvePresetPathFromMaterial(data.material);
        if (!presetPath.empty())
        {
            materialView->Open_Preset(presetPath);
            return;
        }

        materialView->Open_PresetBrowser();
    }

    bool Apply_PresetFileToRequired(const wchar_t* filePath, RequiredModuleData& data)
    {
        if (filePath == nullptr || filePath[0] == L'\0')
            return false;

        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (materialView == nullptr)
            return false;

        materialView->Open_Preset(filePath);
        if (!materialView->Has_SelectedPreset())
            return false;

        data.material = materialView->Get_SelectedPresetMaterial();
        return true;
    }

    bool Draw_MaterialDropSlot(
        const AuthoringEmitter& emitter,
        RequiredModuleData& data,
        EffectMaterialPreviewRenderer* materialSlotPreview,
        float timeDelta,
        bool allowAuthoring)
    {
        constexpr float browseButtonWidth{ 28.f };
        constexpr float slotHeight = 54.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMaterialDragging =
            allowAuthoring &&
            activePayload != nullptr &&
            activePayload->IsDataType(kEffectMaterialPayloadType);

        const ImVec2 slotMin = ImGui::GetCursorScreenPos();
        const float slotWidth = max(180.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        if (!allowAuthoring)
            ImGui::BeginDisabled();
        ImGui::InvisibleButton("##MaterialDropSlot", ImVec2(slotWidth, slotHeight));
        if (!allowAuthoring)
            ImGui::EndDisabled();

        bool changed = false;
        if (allowAuthoring && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                kEffectMaterialPayloadType,
                ImGuiDragDropFlags_AcceptBeforeDelivery
            ))
            {
                if (payload->IsDelivery())
                    changed = Apply_PresetFileToRequired(static_cast<const wchar_t*>(payload->Data), data);
            }

            ImGui::EndDragDropTarget();
        }

        const bool isHovered = ImGui::IsItemHovered(
            allowAuthoring ? ImGuiHoveredFlags_None : ImGuiHoveredFlags_AllowWhenDisabled
        );
        const ImVec2 slotMax{ slotMin.x + slotWidth, slotMin.y + slotHeight };
        const ImU32 fillColor = isMaterialDragging
                                ? IM_COL32(42, 56, 78, 255)
                                : !allowAuthoring ? IM_COL32(26, 28, 34, 255)
                                  : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = isMaterialDragging
                                  ? IM_COL32(96, 165, 250, 255)
                                  : !allowAuthoring ? IM_COL32(58, 64, 76, 255)
                                    : isHovered ? IM_COL32(120, 130, 150, 255) : IM_COL32(70, 78, 92, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 6.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 6.f, 0, isMaterialDragging ? 2.5f : 1.5f);

        ShaderResourceView* previewSrv = nullptr;
        if (materialSlotPreview != nullptr)
        {
            const bool allowScalarParticleLife = Is_MaterialScalarParticleLifeSupported(emitter);
            materialSlotPreview->Update_MaterialTime(timeDelta);
            if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(emitter))
            {
                materialSlotPreview->Render_Material(
                    data.material,
                    Build_CoreColorRgbModulationRuntimeDesc(*modulation),
                    Build_MaterialVec2ModulationRuntimeDesc(*modulation),
                    Build_MaterialScalarModulationRuntimeDesc(*modulation),
                    data.duration,
                    allowScalarParticleLife
                );
            }
            else
                materialSlotPreview->Render_Material(data.material);
            previewSrv = materialSlotPreview->Get_SRV();
        }
        const ImVec2 thumbnailMin{ slotMin.x + 7.f, slotMin.y + 7.f };
        const ImVec2 thumbnailMax{ thumbnailMin.x + 40.f, thumbnailMin.y + 40.f };
        if (previewSrv != nullptr)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSrv));
            drawList->AddImage(textureId, thumbnailMin, thumbnailMax);
        }
        else
        {
            drawList->AddRectFilled(thumbnailMin, thumbnailMax, IM_COL32(18, 20, 24, 255), 4.f);
            drawList->AddText(ImVec2(thumbnailMin.x + 8.f, thumbnailMin.y + 12.f), IM_COL32(150, 160, 176, 255), "MAT");
        }
        drawList->AddRect(thumbnailMin, thumbnailMax, IM_COL32(85, 96, 116, 255), 4.f);

        const string& resolvedName = Resolve_MaterialDisplayName(data);
        const string displayName = resolvedName.empty() ? "(drop material here)" : resolvedName;
        const string blendModeName = string(magic_enum::enum_name(data.material.blendMode));
        const string sourceSummary =
            data.material.blendMode == EffectMaterialBlendMode::Additive
            ? string(magic_enum::enum_name(data.material.additive.colorSource)) + " / " +
              string(magic_enum::enum_name(data.material.additive.amountSource))
            : data.material.opacitySource;
        const string materialSummary = blendModeName + " / " + sourceSummary +
                                       " / SubUV " + to_string(data.material.subUVCols) + " cols x " + to_string(data.material.subUVRows) + " rows";
        drawList->PushClipRect(
            ImVec2(slotMin.x + 57.f, slotMin.y),
            ImVec2(slotMax.x - 8.f, slotMax.y),
            true
        );
        drawList->AddText(ImVec2(slotMin.x + 57.f, slotMin.y + 9.f), IM_COL32(235, 238, 245, 255), displayName.c_str());
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 0.9f,
            ImVec2(slotMin.x + 57.f, slotMin.y + 29.f),
            IM_COL32(145, 154, 172, 255),
            materialSummary.c_str()
        );
        drawList->PopClipRect();

        if (isHovered)
        {
            if (!allowAuthoring)
                ImGui::SetTooltip("Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다.");
            else
            {
                const string mainTooltip =
                    "GUID: " + (data.material.mainTextureGuid.empty() ? string{ "(none)" } : data.material.mainTextureGuid) +
                    "\nPath: " + (data.material.mainTexturePath.empty() ? string{ "(none)" } : data.material.mainTexturePath);
                const string noiseTooltip =
                    data.material.noiseTextureGuid.empty() && data.material.noiseTexturePath.empty()
                    ? string{ "GUID: (none)\nPath: (none)" }
                    : "GUID: " + (data.material.noiseTextureGuid.empty() ? string{ "(none)" } : data.material.noiseTextureGuid) +
                      "\nPath: " + (data.material.noiseTexturePath.empty() ? string{ "(none)" } : data.material.noiseTexturePath);
                ImGui::SetTooltip(
                    "Content Browser의 .effectmaterial.json 머티리얼 파일을 여기에 드롭하면 현재 embedded material instance로 복사합니다.\n"
                    "현재 sprite effect 경로에서는 Main Texture를 emissive color source로, Opacity Source를 별도 opacity 해석 기준으로 사용합니다.\n\n"
                    "Main Texture (Emissive)\n%s\n\nNoise Texture\n%s\n\n%s / %s / SubUV %u cols x %u rows",
                    mainTooltip.c_str(),
                    noiseTooltip.c_str(),
                    blendModeName.c_str(),
                    sourceSummary.c_str(),
                    data.material.subUVCols,
                    data.material.subUVRows
                );
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (!allowAuthoring)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickMaterial", ImVec2{ browseButtonWidth, slotHeight }))
        {
            wstring pickedFilePath{};
            if (Try_PickMaterialPresetDialog(pickedFilePath))
                changed = Apply_PresetFileToRequired(pickedFilePath.c_str(), data);
        }
        if (!allowAuthoring)
            ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(allowAuthoring ? ImGuiHoveredFlags_DelayShort : ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(
                allowAuthoring
                ? "머티리얼 원본 파일 선택"
                : "Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다."
            );
        }

        return changed;
    }

    uint32 Resolve_SubUvFrameCount(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                return 1;

            const uint32 rows = max(1u, requiredData->material.subUVRows);
            const uint32 cols = max(1u, requiredData->material.subUVCols);
            return max(1u, rows * cols);
        }

        return 1;
    }

    bool Is_SameVec3(const Vec3& lhs, const Vec3& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z);
    }

    Vec3 Resolve_CoreColorRgbDefault(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                break;

            return Vec3{
                requiredData->material.coreEmissive.coreColor.x,
                requiredData->material.coreEmissive.coreColor.y,
                requiredData->material.coreEmissive.coreColor.z
            };
        }

        return Vec3{ 1.f, 0.85f, 0.45f };
    }

    MaterialCoreColorRgbModulatorData Make_CoreColorRgbModulator(const AuthoringEmitter& emitter)
    {
        MaterialCoreColorRgbModulatorData data{};
        data.distribution = Vector3DistributionData::Make_Constant(Resolve_CoreColorRgbDefault(emitter));
        return data;
    }

    bool Draw_RadialPivotProperty(
        DetailPropertyContext& detailContext,
        Vec3& value,
        const Vec3& defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RadialPivot");
        detailContext.Draw_PropertyLabel("방사 기준점");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip(
                "입자 생성 위치에서 이 기준점을 뺀 방향으로 초기 속도를 더합니다.\n"
                "기준점이 생성 위치보다 위에 있으면 +속도에서도 아래 방향으로 퍼질 수 있습니다."
            );
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat3("##Value", &value.x, 0.01f);
        if (detailContext.Draw_ResetButton(!Is_SameVec3(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_RadialWorldSpaceProperty(
        DetailPropertyContext& detailContext,
        bool& value,
        bool defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RadialWorldSpace");
        detailContext.Draw_PropertyLabel("월드 스페이스");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip(
                "켜면 방사 기준점을 world offset으로 사용합니다.\n"
                "끄면 emitter 회전 기준 local offset으로 변환합니다."
            );
        }

        changed |= ImGui::Checkbox("##Value", &value);
        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    const char* Get_BlendModeLabel(EffectMaterialBlendMode value)
    {
        switch (value)
        {
        case EffectMaterialBlendMode::AlphaBlend:
            return "AlphaBlend";
        case EffectMaterialBlendMode::Additive:
            return "Additive";
        case EffectMaterialBlendMode::Masked:
            return "Masked";
        case EffectMaterialBlendMode::Modulate:
            return "Modulate";
        default:
            return "AlphaBlend";
        }
    }

    const char* Get_SortPolicyLabel(EmitterSortPolicy value)
    {
        switch (value)
        {
        case EmitterSortPolicy::EmitterDepth:
            return "Emitter Depth";
        case EmitterSortPolicy::None:
        default:
            return "None";
        }
    }

    bool Draw_BlendModeProperty(
        DetailPropertyContext& detailContext,
        const AuthoringEmitter& emitter,
        EffectMaterialBlendMode& value,
        EffectMaterialBlendMode defaultValue)
    {
        static constexpr EffectMaterialBlendMode kModes[]{
            EffectMaterialBlendMode::AlphaBlend,
            EffectMaterialBlendMode::Additive,
            EffectMaterialBlendMode::Masked,
        };

        const bool trailEmitter = emitter.typeData.kind == AuthoringTypeDataKind::Trail;
        bool changed = false;

        ImGui::PushID("RequiredBlendMode");
        detailContext.Draw_PropertyLabel("Blend Mode");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_BlendModeLabel(value)))
        {
            for (const EffectMaterialBlendMode mode : kModes)
            {
                const bool selectable = !(trailEmitter && mode == EffectMaterialBlendMode::Masked);
                const bool selected = value == mode;

                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_BlendModeLabel(mode), selected) && selectable)
                {
                    value = mode;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_SortPolicyProperty(
        DetailPropertyContext& detailContext,
        EmitterSortPolicy& value,
        EmitterSortPolicy defaultValue)
    {
        static constexpr EmitterSortPolicy kPolicies[]{
            EmitterSortPolicy::None,
            EmitterSortPolicy::EmitterDepth,
        };

        bool changed = false;

        ImGui::PushID("RequiredSortPolicy");
        detailContext.Draw_PropertyLabel("Sort Policy");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SortPolicyLabel(value)))
        {
            for (const EmitterSortPolicy policy : kPolicies)
            {
                const bool selected = value == policy;
                if (ImGui::Selectable(Get_SortPolicyLabel(policy), selected))
                {
                    value = policy;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_IntProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        int32& value,
        int32 defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int inputValue = value;
        if (ImGui::InputInt("##Value", &inputValue, 1, 10))
        {
            value = static_cast<int32>(inputValue);
            changed = true;
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }
}

bool RequiredModuleDetail::Draw(
    AuthoringEmitter& emitter,
    const AuthoringModule& module,
    RequiredModuleData& data,
    DetailPropertyContext& detailContext,
    float timeDelta)
{
    const RequiredModuleData defaultData{};
    bool changed = false;
    const AuthoringModuleType moduleType = module.type;

    if (_materialSlotPreview == nullptr)
        _materialSlotPreview = EffectMaterialPreviewRenderer::Create(64);

    const bool allowRequiredMaterialAuthoring = !Is_MeshEffectAuthoring(emitter);

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredMaterial"))
        {
            ImGui::PushID("MaterialDropSlot");
            detailContext.Draw_PropertyLabel(
                "머티리얼",
                allowRequiredMaterialAuthoring
                ? "이 emitter의 기본 material instance입니다. Mesh 계열은 Mesh TypeData Assigned Material이 편집 정본입니다."
                : "Mesh 계열 material은 Mesh TypeData의 Assigned Material에서 편집합니다."
            );
            changed |= Draw_MaterialDropSlot(
                emitter,
                data,
                _materialSlotPreview.get(),
                timeDelta,
                allowRequiredMaterialAuthoring
            );
            detailContext.Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialEdit");
            detailContext.Draw_PropertyLabel("작업", "material instance 편집 창을 열거나 원본 preset을 확인합니다.");

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float editButtonWidth = (availableWidth - spacing) * 0.5f;
            const float sourceButtonWidth = (availableWidth - spacing) * 0.5f;

            if (!allowRequiredMaterialAuthoring)
                ImGui::BeginDisabled();
            if (ImGui::Button("인스턴스 편집", ImVec2{ editButtonWidth, 24.f }))
                Open_MaterialInstanceWindow(emitter.id, module.id);
            if (!allowRequiredMaterialAuthoring)
                ImGui::EndDisabled();
            if (!allowRequiredMaterialAuthoring && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다.");

            ImGui::SameLine();

            if (!allowRequiredMaterialAuthoring)
                ImGui::BeginDisabled();
            if (ImGui::Button("원본 보기", ImVec2{ sourceButtonWidth, 24.f }))
                Open_PresetBrowserWindow(data);
            if (!allowRequiredMaterialAuthoring)
                ImGui::EndDisabled();

            if (ImGui::IsItemHovered(allowRequiredMaterialAuthoring ? ImGuiHoveredFlags_DelayShort : ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip(
                    allowRequiredMaterialAuthoring
                    ? "원본 머티리얼을 확인"
                    : "Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다."
                );
            }
            ImGui::PopID();

            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTransform"))
        {
            changed |= detailContext.Draw_Vec3Property(
                "원점",
                data.emitterOrigin,
                defaultData.emitterOrigin,
                0.01f,
                nullptr,
                "emitter의 authoring 기준 local origin입니다."
            );
            changed |= detailContext.Draw_Vec3Property(
                "회전 (도)",
                data.emitterRotationDegrees,
                defaultData.emitterRotationDegrees,
                0.01f,
                nullptr,
                "emitter 기준 local 회전입니다. 단위는 degree입니다."
            );
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredSpaceSort"))
        {
            const bool isSourceHistorySpriteTrail = emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
            changed |= isSourceHistorySpriteTrail
                       ? detailContext.Draw_SourceHistorySpriteTrailScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment)
                       : detailContext.Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment);
            if (Uses_DirectionalAlignmentOptions(data.screenAlignment))
            {
                const EmitterDirectionalAlignmentMode oldMode = data.directionalAlignmentMode;

                if (detailContext.Draw_DirectionalAlignmentModeProperty("방향 정렬 방식", data.directionalAlignmentMode, defaultData.directionalAlignmentMode))
                {
                    changed = true;
                    if (oldMode != data.directionalAlignmentMode)
                        Notify_DirectionalAlignmentModeChanged(data.screenAlignment, data.directionalAlignmentMode);
                }
            }

            const bool showSpriteTextureAxis =
                isSourceHistorySpriteTrail
                ? Uses_SourceHistorySpriteTrailCardAxis(data.screenAlignment, data.directionalAlignmentMode)
                : Uses_DirectionalAlignmentOptions(data.screenAlignment) &&
                  data.directionalAlignmentMode == EmitterDirectionalAlignmentMode::TextureAxis;
            if (showSpriteTextureAxis)
            {
                const EmitterSpriteTextureAxis oldAxis = data.spriteTextureAxis;
                if (isSourceHistorySpriteTrail)
                {
                    if (detailContext.Draw_SpriteTextureAxisProperty(
                        "카드 길이 축",
                        data.spriteTextureAxis,
                        defaultData.spriteTextureAxis,
                        "Texture X를 SourceHistorySpriteTrail 카드 길이 방향으로 사용합니다.",
                        "Texture Y를 SourceHistorySpriteTrail 카드 길이 방향으로 사용합니다."
                    ))
                    {
                        changed = true;
                        if (oldAxis != data.spriteTextureAxis)
                            Notify_TextureAxisChanged(data.spriteTextureAxis);
                    }
                }
                else if (detailContext.Draw_SpriteTextureAxisProperty("정렬할 텍스처 축", data.spriteTextureAxis, defaultData.spriteTextureAxis))
                {
                    changed = true;
                    if (oldAxis != data.spriteTextureAxis)
                        Notify_TextureAxisChanged(data.spriteTextureAxis);
                }

                changed |= detailContext.Draw_FloatProperty(
                    "롤 보정 (도)",
                    data.spriteRollOffsetDegrees,
                    defaultData.spriteRollOffsetDegrees,
                    0.1f,
                    "정렬된 sprite/card에 추가로 더하는 roll 보정값입니다. 단위는 degree입니다."
                );
            }

            changed |= detailContext.Draw_BoolProperty(
                "로컬 스페이스 사용",
                data.useLocalSpace,
                defaultData.useLocalSpace,
                "켜면 particle 위치/속도를 emitter local space 기준으로 유지합니다. 끄면 spawn 이후 world 기준으로 움직입니다."
            );
            detailContext.End_PropertyTable();
        }

        if (Uses_DirectionalAlignmentOptions(data.screenAlignment))
        {
            if (data.directionalAlignmentMode == EmitterDirectionalAlignmentMode::TextureAxis)
                ImGui::TextDisabled("Texture Axis는 world X/Y/Z가 아니라 texture 가로/세로 축입니다. 나머지 축은 카메라 가시성을 유지합니다.");
            else
                ImGui::TextDisabled("Look Direction은 기준 방향을 sprite 카드의 바라보는 방향으로 사용합니다.");
        }

        detailContext.Draw_ScreenAlignmentImplementationNote(data.screenAlignment);
    }

    if (ImGui::CollapsingHeader("렌더링 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredRenderingSort"))
        {
            changed |= Draw_BlendModeProperty(detailContext, emitter, data.material.blendMode, defaultData.material.blendMode);
            changed |= Draw_SortPolicyProperty(detailContext, data.sortPolicy, defaultData.sortPolicy);
            changed |= Draw_IntProperty(detailContext, "Sort Layer", data.sortLayer, defaultData.sortLayer);
            detailContext.End_PropertyTable();
        }

        if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
            ImGui::TextDisabled("Trail은 Masked 선택을 막고 AlphaBlend/Additive 경로를 유지합니다.");

        if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (detailContext.Begin_PropertyTable("RequiredRenderingSortAdvanced"))
            {
                changed |= detailContext.Draw_FloatProperty(
                    "Sort Bias",
                    data.sortBias,
                    defaultData.sortBias,
                    0.01f,
                    "같은 Sort Layer와 같은 Sort Policy 안에서만 마지막 보정값으로 사용합니다."
                );
                detailContext.End_PropertyTable();
            }
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("이미터 재생 / 스폰 시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTime"))
        {
            changed |= detailContext.Draw_UintProperty("루프 횟수", data.loopCount, defaultData.loopCount, "emitter spawn loop를 반복할 횟수입니다. 0은 무한 반복 의미로 사용됩니다.");
            changed |= detailContext.Draw_FloatProperty(
                "스폰 활성 시간",
                data.duration,
                defaultData.duration,
                0.01f,
                "새 particle을 만들 수 있는 emitter loop 구간입니다. particle 개별 수명은 Lifetime 모듈에서 정합니다.",
                Find_AuthoringValueRange(moduleType, "duration")
            );
            changed |= detailContext.Draw_FloatProperty(
                "스폰 시작 딜레이",
                data.delay,
                defaultData.delay,
                0.01f,
                "loop 시작 후 spawn을 시작하기 전까지 기다리는 시간입니다.",
                Find_AuthoringValueRange(moduleType, "delay")
            );
            changed |= detailContext.Draw_BoolProperty(
                "첫 루프만 스폰 딜레이",
                data.delayFirstLoopOnly,
                defaultData.delayFirstLoopOnly,
                "켜면 delay를 첫 loop에만 적용하고 이후 반복에서는 바로 spawn합니다."
            );
            detailContext.End_PropertyTable();
        }

        ImGui::TextDisabled("스폰 활성 시간은 새 파티클을 만들 수 있는 emitter loop 구간입니다. 이미 태어난 파티클은 Particle Lifetime이 끝날 때까지 남을 수 있습니다.");
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredKill"))
        {
            changed |= detailContext.Draw_BoolProperty(
                "비활성화 시 킬 (외부 비활성화 계약 보류)",
                data.killOnDeactivate,
                defaultData.killOnDeactivate,
                "외부 비활성화 이벤트가 들어왔을 때 active particle을 즉시 종료할지 정합니다. 현재 외부 계약은 보류 상태입니다."
            );
            changed |= detailContext.Draw_BoolProperty(
                "완료 시 킬",
                data.killOnCompleted,
                defaultData.killOnCompleted,
                "emitter spawn loop가 완료될 때 이미 살아 있는 particle도 즉시 종료합니다."
            );
            detailContext.End_PropertyTable();
        }

        if (data.killOnCompleted)
            ImGui::TextDisabled("완료 시 active particle도 즉시 종료합니다.");
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= detailContext.Draw_BoolProperty(
                "최대 드로우 수 사용",
                data.useMaxDrawCount,
                defaultData.useMaxDrawCount,
                "renderer에 제출할 최대 particle/card 수 제한을 사용할지 정합니다."
            );
            changed |= detailContext.Draw_UintProperty("최대 드로우 수", data.maxDrawCount, defaultData.maxDrawCount, "한 frame에 그릴 수 있는 최대 particle/card 수입니다.");
            detailContext.End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = emitter.typeData.kind == AuthoringTypeDataKind::None
                               ? data.rendererType
                               : Authoring::Resolve_RendererType(emitter.typeData);
        emitter.textureId = data.material.mainTexturePath;
        emitter.resourceSummary = data.material.mainTexturePath;
    }

    return changed;
}

bool SpawnModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, SpawnModuleData& data, DetailPropertyContext& detailContext)
{
    const SpawnModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnRate"))
        {
            changed |= detailContext.Draw_BoolProperty(
                "연속 스폰 사용",
                data.processSpawnRate,
                defaultData.processSpawnRate,
                "켜면 emitter loop 동안 초당 생성 수 기준으로 계속 particle을 만듭니다."
            );
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "초당 생성 수",
            data.spawnRate,
            defaultData.spawnRate,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "spawnRate"),
            "emitter loop가 활성인 동안 초당 생성할 particle 수입니다."
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "스폰 속도 스케일",
            data.spawnRateScale,
            defaultData.spawnRateScale,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "spawnRateScale"),
            "초당 생성 수에 곱하는 추가 배율입니다. 1이면 원래 spawn rate를 유지합니다."
        );
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnBurst"))
        {
            changed |= detailContext.Draw_BoolProperty(
                "버스트 사용",
                data.processBurstList,
                defaultData.processBurstList,
                "켜면 loop timeline의 지정 시점에 묶음 particle을 생성합니다."
            );
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "버스트 스케일",
            data.burstScale,
            defaultData.burstScale,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "burstScale"),
            "Burst List의 생성 개수에 곱하는 배율입니다."
        );
        changed |= detailContext.Draw_BurstListProperty(data.burstList, defaultData.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnLimit"))
        {
            changed |= detailContext.Draw_UintProperty(
                "최대 활성 파티클 수",
                data.maxParticleCount,
                defaultData.maxParticleCount,
                "동시에 살아 있을 수 있는 particle 수 상한입니다. spawn rate와 burst 결과가 이 값을 넘으면 제한됩니다."
            );
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

bool LifetimeModuleDetail::Draw(const AuthoringEmitter& emitter, const AuthoringModule& module, LifetimeModuleData& data, DetailPropertyContext& detailContext)
{
    const LifetimeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("파티클 생존 시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const bool supportsLifetimeConstantCurve =
            emitter.typeData.kind != AuthoringTypeDataKind::Trail &&
            emitter.typeData.kind != AuthoringTypeDataKind::Ribbon;
        changed |= detailContext.Draw_FloatDistributionGroup(
            "파티클 생존 시간",
            data.lifeTime,
            defaultData.lifeTime,
            0.01f,
            true,
            supportsLifetimeConstantCurve,
            Find_AuthoringValueRange(module.type, "lifeTime"),
            "spawn window가 아니라 태어난 particle 개별 생존 시간입니다."
        );
        ImGui::TextDisabled("이미 생성된 particle 또는 visual life를 생애 중간에 다시 바꾸는 Over Life curve가 아닙니다.");
        if (emitter.typeData.kind == AuthoringTypeDataKind::Beam)
        {
            ImGui::TextDisabled("Beam ConstantCurve: 전체 반복 순서 진행률로 각 loop의 visual life를 한 번 샘플합니다.");
            ImGui::TextDisabled("Beam 무한 반복: 반복 번호를 curve 구간에 순환 적용합니다.");
        }
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
            ImGui::TextDisabled("Mesh ConstantCurve: particle spawn 시점의 현재 loop 진행률로 최대 수명을 한 번 샘플합니다.");
        else if (emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
            ImGui::TextDisabled("Sprite Trail ConstantCurve: stamp 생성 시점의 현재 loop 진행률로 stamp lifetime을 한 번 샘플합니다.");
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
            ImGui::TextDisabled("Trail ConstantCurve: segment별 lifetime 저장이 없어 아직 비지원입니다. Constant/Uniform 수명은 계속 지원됩니다.");
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon)
            ImGui::TextDisabled("Ribbon ConstantCurve: sample별 lifetime 저장이 없어 아직 비지원입니다. Constant/Uniform 수명은 계속 지원됩니다.");
        else
            ImGui::TextDisabled("Sprite ConstantCurve: particle spawn 시점의 현재 loop 진행률로 최대 수명을 한 번 샘플합니다.");
    }

    return changed;
}

bool InitialSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector2DistributionGroup("크기", data.size, defaultData.size, 0.01f, true, nullptr, "spawn 시점에 한 번 샘플되는 sprite 초기 크기입니다.");

    return changed;
}

bool InitialMeshSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialMeshSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialMeshSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("메시 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector3DistributionGroup(
            "크기 XYZ",
            data.size,
            defaultData.size,
            0.01f,
            true,
            false,
            nullptr,
            "spawn 시점에 한 번 샘플되는 mesh 초기 크기입니다."
        );
    }

    return changed;
}

bool InitialLocationModuleDetail::Draw(const AuthoringEmitter& emitter, AuthoringModule&, InitialLocationModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialLocationModuleData defaultData{};
    bool changed = false;
    const bool isRibbon = emitter.typeData.kind == AuthoringTypeDataKind::Ribbon;

    if (ImGui::CollapsingHeader("위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector3DistributionGroup(
            "위치",
            data.location,
            defaultData.location,
            0.01f,
            false,
            true,
            nullptr,
            isRibbon
            ? "Ribbon이 source history Head에 더하는 emitter local offset입니다."
            : "spawn 시점에 한 번 샘플되는 emitter origin 기준 local offset입니다."
        );
    }

    return changed;
}

bool RibbonOrientationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, RibbonOrientationModuleData& data, DetailPropertyContext& detailContext)
{
    const RibbonOrientationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("리본 방향", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RibbonOrientation"))
        {
            changed |= Draw_RibbonSpreadBasisProperty(
                detailContext,
                data.spreadBasis,
                defaultData.spreadBasis
            );
            changed |= detailContext.Draw_FloatProperty(
                "펼침 각도",
                data.spreadAngleDegrees,
                defaultData.spreadAngleDegrees,
                0.1f,
                "선택한 펼침 기준으로 만든 폭 방향을 리본 tangent 축 기준으로 추가 회전하는 각도입니다."
            );
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

bool SphereLocationModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, SphereLocationModuleData& data, DetailPropertyContext& detailContext)
{
    const SphereLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("구 위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SphereLocation"))
        {
            changed |= detailContext.Draw_Vec3Property("오프셋", data.offset, defaultData.offset, 0.01f, nullptr, "구 분포 중심에 더하는 emitter local offset입니다.");
            changed |= detailContext.Draw_FloatProperty(
                "반지름",
                data.radius,
                defaultData.radius,
                0.01f,
                "구 분포의 반지름입니다. Surface/Volume 생성 방식의 기준 크기입니다.",
                Find_AuthoringValueRange(module.type, "radius")
            );
            changed |= Draw_SphereLocationSpawnModeProperty(
                detailContext,
                "생성 방식",
                data.spawnMode,
                defaultData.spawnMode
            );
            const bool placementModeEnabled = data.spawnMode == SphereLocationSpawnMode::Surface;
            changed |= Draw_SphereLocationPlacementModeProperty(
                detailContext,
                "표면 배치",
                data.placementMode,
                defaultData.placementMode,
                placementModeEnabled
            );
            detailContext.End_PropertyTable();
        }

        changed |= Draw_RandomSeedProperty(detailContext, data.randomSeed);
    }

    data.radius = max(0.f, data.radius);
    return changed;
}

bool PlaneRadialLocationModuleDetail::Draw(
    AuthoringEmitter&,
    const AuthoringModule& module,
    PlaneRadialLocationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const PlaneRadialLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("평면/방사 위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("PlaneRadialLocation"))
        {
            changed |= Draw_PlaneRadialLocationPlaneProperty(
                detailContext,
                "기준 평면",
                data.plane,
                defaultData.plane
            );
            changed |= Draw_PlaneRadialLocationShapeProperty(
                detailContext,
                "형태",
                data.shape,
                defaultData.shape
            );
            changed |= detailContext.Draw_Vec3Property("중심 오프셋", data.offset, defaultData.offset, 0.01f, nullptr, "평면/방사 분포 중심에 더하는 emitter local offset입니다.");
            changed |= detailContext.Draw_FloatProperty(
                "두께",
                data.thickness,
                defaultData.thickness,
                0.01f,
                "기준 평면의 normal 방향으로 더할 위치 범위입니다. runtime sampling에서 음수는 절댓값 기준으로 정규화할 예정입니다."
            );
            detailContext.End_PropertyTable();
        }

        if (data.shape == PlaneRadialLocationShape::Rectangle)
        {
            changed |= detailContext.Draw_FloatDistributionGroup(
                "U 분포",
                data.uDistribution,
                defaultData.uDistribution,
                0.01f,
                true,
                false,
                nullptr,
                "Rectangle 기준 평면의 U축 위치를 spawn 시점에 한 번 샘플합니다."
            );
            changed |= detailContext.Draw_FloatDistributionGroup(
                "V 분포",
                data.vDistribution,
                defaultData.vDistribution,
                0.01f,
                true,
                false,
                nullptr,
                "Rectangle 기준 평면의 V축 위치를 spawn 시점에 한 번 샘플합니다."
            );
        }
        else
        {
            changed |= detailContext.Draw_FloatDistributionGroup(
                "반지름 분포",
                data.radiusDistribution,
                defaultData.radiusDistribution,
                0.01f,
                true,
                false,
                Find_AuthoringValueRange(module.type, "radiusDistribution"),
                "Disc 기준 중심에서 떨어진 반지름을 spawn 시점에 한 번 샘플합니다."
            );
            changed |= detailContext.Draw_FloatDistributionGroup(
                "각도 분포",
                data.angleDegreesDistribution,
                defaultData.angleDegreesDistribution,
                0.01f,
                true,
                false,
                nullptr,
                "Disc 기준 평면 안의 각도를 degree 단위로 spawn 시점에 한 번 샘플합니다."
            );
        }

        ImGui::TextDisabled("위치 분포는 spawn 시점 1회 샘플이라 현재 상수/균등만 지원합니다.");
        if (data.shape == PlaneRadialLocationShape::Disc)
            ImGui::TextDisabled("원반 채움은 보통 반지름 최소값 0을 사용합니다.");

        if (ImGui::TreeNodeEx("고급", ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            const bool placementModeEnabled = Is_PlaneRadialLocationAdvancedPlacementShape(data.shape);
            if (detailContext.Begin_PropertyTable("PlaneRadialLocationAdvanced"))
            {
                changed |= Draw_PlaneRadialLocationPlacementModeProperty(
                    detailContext,
                    "배치 방식",
                    data.placementMode,
                    defaultData.placementMode,
                    placementModeEnabled
                );
                detailContext.End_PropertyTable();
            }

            if (!placementModeEnabled)
                ImGui::TextDisabled("배치 방식은 Rectangle에서는 사용되지 않습니다.");

            ImGui::TreePop();
        }
    }

    return changed;
}

bool CylinderLocationModuleDetail::Draw(
    AuthoringEmitter&,
    const AuthoringModule& module,
    CylinderLocationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const CylinderLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("실린더 위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("CylinderLocation"))
        {
            changed |= Draw_CylinderLocationAxisProperty(
                detailContext,
                "길이축",
                data.axis,
                defaultData.axis
            );
            changed |= Draw_CylinderLocationSpawnModeProperty(
                detailContext,
                "생성 방식",
                data.spawnMode,
                defaultData.spawnMode
            );
            changed |= Draw_CylinderLocationPlacementModeProperty(
                detailContext,
                "원주 배치",
                data.placementMode,
                defaultData.placementMode
            );
            changed |= detailContext.Draw_Vec3Property(
                "중심 오프셋",
                data.offset,
                defaultData.offset,
                0.01f,
                nullptr,
                "원통 분포 중심에 더하는 emitter local offset입니다."
            );
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "반지름 분포",
            data.radiusDistribution,
            defaultData.radiusDistribution,
            0.01f,
            true,
            false,
            Find_AuthoringValueRange(module.type, "radiusDistribution"),
            "원통 중심축에서 떨어진 반지름을 spawn 시점에 한 번 샘플합니다."
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "높이 분포",
            data.heightDistribution,
            defaultData.heightDistribution,
            0.01f,
            true,
            false,
            nullptr,
            "원통 길이축 방향 위치를 spawn 시점에 한 번 샘플합니다."
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "각도 분포",
            data.angleDegreesDistribution,
            defaultData.angleDegreesDistribution,
            0.01f,
            true,
            false,
            nullptr,
            "원주 각도를 degree 단위로 spawn 시점에 한 번 샘플합니다."
        );

        ImGui::TextDisabled("위치 분포는 spawn 시점 1회 샘플이라 현재 상수/균등만 지원합니다.");
        if (data.spawnMode == CylinderLocationSpawnMode::SideSurface)
            ImGui::TextDisabled("반지름 최소/최대가 다르면 두께 있는 원통 shell로 해석합니다.");
    }

    return changed;
}

bool InitialVelocityModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialVelocityModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialVelocityModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("InitialVelocity"))
        {
            changed |= detailContext.Draw_BoolProperty(
                "월드 스페이스",
                data.inWorldSpace,
                defaultData.inWorldSpace,
                "켜면 초기 속도 벡터를 world 기준으로, 끄면 emitter local 기준으로 해석합니다."
            );
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup(
            "속도",
            data.velocity,
            defaultData.velocity,
            0.01f,
            true,
            false,
            nullptr,
            "spawn 시점에 한 번 샘플되는 초기 속도 벡터입니다."
        );
    }

    return changed;
}

bool InitialRadialVelocityModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    InitialRadialVelocityModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialRadialVelocityModuleData defaultData{};
    bool changed = false;

    const bool radialVelocityOpen = ImGui::CollapsingHeader("방사성 속도", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip(
            "생성 위치와 방사 기준점의 방향으로 더하는 초기 속도입니다.\n"
            "중앙 발산/흡입형 sprite streak의 1차 후보입니다.\n"
            "양수는 기준점에서 멀어지고, 음수는 기준점 쪽으로 이동합니다.\n"
            "지속 attractor가 아니라 생성 시점의 속도 항입니다."
        );
    }

    if (radialVelocityOpen)
    {
        if (detailContext.Begin_PropertyTable("InitialRadialVelocity"))
        {
            changed |= Draw_RadialPivotProperty(detailContext, data.radialPivot, defaultData.radialPivot);
            changed |= Draw_RadialWorldSpaceProperty(detailContext, data.inWorldSpace, defaultData.inWorldSpace);
            changed |= Draw_InitialRadialVelocityCenterDirectionModeProperty(
                detailContext,
                data.centerDirectionMode,
                defaultData.centerDirectionMode
            );
            detailContext.End_PropertyTable();
        }

        if (data.centerDirectionMode == InitialRadialVelocityCenterDirectionMode::PlaneRadial)
        {
            ImGui::TextDisabled("평면 방사는 같은 emitter의 평면/방사 위치 방향을 사용합니다.");
            if (!Has_PlaneRadialLocationModule(emitter))
                ImGui::TextDisabled("평면/방사 위치가 없으면 랜덤 상승 방향으로 처리됩니다.");
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "속도",
            data.speed,
            defaultData.speed,
            0.01f,
            true,
            true,
            nullptr,
            "방사 방향에 곱해지는 spawn 시점 초기 속도 크기입니다. 음수는 기준점 쪽으로 향합니다."
        );
    }

    return changed;
}

bool VelocityConeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    VelocityConeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const VelocityConeModuleData defaultData{};
    bool changed = false;

    const bool coneVelocityOpen = ImGui::CollapsingHeader("속도 원뿔", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip(
            "생성 위치와 무관하게 축 기준 원뿔 안에서 뽑은 방향으로 더하는 초기 속도입니다.\n"
            "InitialVelocity, InitialRadialVelocity와 누적됩니다.\n"
            "각도는 중심축 기준 half-angle입니다."
        );
    }

    if (coneVelocityOpen)
    {
        if (detailContext.Begin_PropertyTable("VelocityCone"))
        {
            changed |= detailContext.Draw_Vec3Property("축", data.axis, defaultData.axis, 0.01f, nullptr, "속도 원뿔의 중심축입니다.");
            changed |= detailContext.Draw_FloatProperty(
                "각도",
                data.angleDegrees,
                defaultData.angleDegrees,
                0.1f,
                "중심축 기준 half-angle입니다. 0이면 축 방향, 180이면 전 방향에 가깝습니다."
            );
            changed |= detailContext.Draw_BoolProperty(
                "월드 스페이스",
                data.inWorldSpace,
                defaultData.inWorldSpace,
                "켜면 원뿔 축을 world 기준으로, 끄면 emitter local 기준으로 해석합니다."
            );
            detailContext.End_PropertyTable();
        }

        const float clampedAngle = clamp(data.angleDegrees, 0.f, 180.f);
        if (data.angleDegrees != clampedAngle)
        {
            data.angleDegrees = clampedAngle;
            changed = true;
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "속도",
            data.speed,
            defaultData.speed,
            0.01f,
            true,
            true,
            nullptr,
            "원뿔 안에서 뽑은 방향에 곱해지는 spawn 시점 초기 속도 크기입니다."
        );
    }

    return changed;
}

bool SourceMotionVelocityModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SourceMotionVelocityModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SourceMotionVelocityModuleData defaultData{};
    bool changed = false;

    const auto direction_mode_label = [](SourceMotionVelocityDirectionMode mode)
    {
        switch (mode)
        {
        case SourceMotionVelocityDirectionMode::InheritSourceVelocity:
            return "소스 속도 방향";
        case SourceMotionVelocityDirectionMode::SourceVelocityDirection:
            return "소스 속도 방향";
        case SourceMotionVelocityDirectionMode::SourceVelocityOpposite:
            return "소스 속도 반대";
        case SourceMotionVelocityDirectionMode::TrailTangent:
            return "트레일 접선";
        case SourceMotionVelocityDirectionMode::TrailTangentOpposite:
            return "트레일 접선 반대";
        case SourceMotionVelocityDirectionMode::SideFromTangent:
            return "접선 측면";
        case SourceMotionVelocityDirectionMode::RandomSideFromTangent:
            return "랜덤 접선 측면";
        default:
            return "트레일 접선";
        }
    };
    const auto direction_mode_tooltip = [](SourceMotionVelocityDirectionMode mode)
    {
        switch (mode)
        {
        case SourceMotionVelocityDirectionMode::InheritSourceVelocity:
            return "기존 asset 호환용 값입니다. 편집 시 소스 속도 방향으로 정규화됩니다.";
        case SourceMotionVelocityDirectionMode::SourceVelocityDirection:
            return "source가 움직인 방향으로 초기 속도를 더합니다.\n속도=0, 소스 속도 상속 배율=1이면 source 속도를 그대로 상속합니다.";
        case SourceMotionVelocityDirectionMode::SourceVelocityOpposite:
            return "source가 움직인 방향의 반대로 초기 속도를 더합니다.\n일반 Sprite/Mesh에서는 emitter center 이동 방향을 사용합니다.";
        case SourceMotionVelocityDirectionMode::TrailTangent:
            return "SourceHistorySpriteTrail의 source history 접선 방향을 사용합니다.\n일반 Sprite/Mesh에는 별도 trail 접선이 없으므로 소스 속도 방향 모드를 우선 사용하세요.";
        case SourceMotionVelocityDirectionMode::TrailTangentOpposite:
            return "SourceHistorySpriteTrail의 source history 접선 반대 방향을 사용합니다.\n일반 Sprite/Mesh에는 별도 trail 접선이 없습니다.";
        case SourceMotionVelocityDirectionMode::SideFromTangent:
            return "SourceHistorySpriteTrail 접선의 측면 방향으로 초기 속도를 더합니다.\n일반 Sprite/Mesh에는 별도 trail 접선이 없습니다.";
        case SourceMotionVelocityDirectionMode::RandomSideFromTangent:
            return "SourceHistorySpriteTrail 접선의 양쪽 측면 중 하나를 랜덤으로 골라 초기 속도를 더합니다.\n일반 Sprite/Mesh에는 별도 trail 접선이 없습니다.";
        default:
            return "";
        }
    };
    const auto is_trail_tangent_mode = [](SourceMotionVelocityDirectionMode mode)
    {
        return mode == SourceMotionVelocityDirectionMode::TrailTangent ||
               mode == SourceMotionVelocityDirectionMode::TrailTangentOpposite ||
               mode == SourceMotionVelocityDirectionMode::SideFromTangent ||
               mode == SourceMotionVelocityDirectionMode::RandomSideFromTangent;
    };

    const bool sourceMotionVelocityOpen = ImGui::CollapsingHeader("소스 모션 속도", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip(
            "source/trail motion에서 얻은 방향과 속도 크기를 spawn 시점 초기 속도에 더합니다.\n"
            "InitialVelocity, InitialRadialVelocity, VelocityCone과 누적됩니다."
        );
    }

    if (sourceMotionVelocityOpen)
    {
        const bool sourceHistorySpriteTrail =
            emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;

        if (detailContext.Begin_PropertyTable("SourceMotionVelocity"))
        {
            ImGui::PushID("DirectionMode");
            detailContext.Draw_PropertyLabel(
                "방향",
                "Source motion에서 초기 속도 방향을 고르는 방식입니다. 항목 위에 마우스를 올리면 타입별 의미를 볼 수 있습니다."
            );
            SourceMotionVelocityDirectionMode nextDirectionMode = data.directionMode;
            if (ImGui::BeginCombo("##Value", direction_mode_label(nextDirectionMode)))
            {
                static constexpr SourceMotionVelocityDirectionMode sourceHistoryCandidates[] = {
                    SourceMotionVelocityDirectionMode::TrailTangent,
                    SourceMotionVelocityDirectionMode::TrailTangentOpposite,
                    SourceMotionVelocityDirectionMode::SideFromTangent,
                    SourceMotionVelocityDirectionMode::RandomSideFromTangent,
                    SourceMotionVelocityDirectionMode::SourceVelocityDirection,
                    SourceMotionVelocityDirectionMode::SourceVelocityOpposite,
                };
                static constexpr SourceMotionVelocityDirectionMode sourceVelocityCandidates[] = {
                    SourceMotionVelocityDirectionMode::SourceVelocityDirection,
                    SourceMotionVelocityDirectionMode::SourceVelocityOpposite,
                };
                const SourceMotionVelocityDirectionMode* candidates =
                    sourceHistorySpriteTrail ? sourceHistoryCandidates : sourceVelocityCandidates;
                const size_t candidateCount =
                    sourceHistorySpriteTrail
                    ? sizeof(sourceHistoryCandidates) / sizeof(sourceHistoryCandidates[0])
                    : sizeof(sourceVelocityCandidates) / sizeof(sourceVelocityCandidates[0]);
                for (size_t index = 0; index < candidateCount; ++index)
                {
                    const SourceMotionVelocityDirectionMode candidate = candidates[index];
                    const bool selected = nextDirectionMode == candidate;
                    if (ImGui::Selectable(direction_mode_label(candidate), selected))
                        nextDirectionMode = candidate;
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("%s", direction_mode_tooltip(candidate));
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", direction_mode_tooltip(nextDirectionMode));
            if (data.directionMode != nextDirectionMode)
            {
                data.directionMode = nextDirectionMode;
                changed = true;
            }
            ImGui::PopID();

            changed |= detailContext.Draw_FloatProperty(
                "소스 속도 상속 배율",
                data.sourceSpeedScale,
                defaultData.sourceSpeedScale,
                0.01f,
                "sourceSpeed에 곱해 초기 속도 크기에 추가하는 배율입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "퍼짐 각도",
                data.spreadAngleDegrees,
                defaultData.spreadAngleDegrees,
                0.1f,
                "선택한 source motion 방향 주변으로 분산할 half-angle입니다."
            );
            detailContext.End_PropertyTable();
        }

        if (!sourceHistorySpriteTrail && is_trail_tangent_mode(data.directionMode))
            ImGui::TextDisabled("트레일 접선 계열은 SourceHistorySpriteTrail에서만 명확한 접선 기준을 가집니다.");

        const float clampedSourceSpeedScale = max(0.f, data.sourceSpeedScale);
        if (data.sourceSpeedScale != clampedSourceSpeedScale)
        {
            data.sourceSpeedScale = clampedSourceSpeedScale;
            changed = true;
        }

        const float clampedSpreadAngle = clamp(data.spreadAngleDegrees, 0.f, 180.f);
        if (data.spreadAngleDegrees != clampedSpreadAngle)
        {
            data.spreadAngleDegrees = clampedSpreadAngle;
            changed = true;
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "속도",
            data.speed,
            defaultData.speed,
            0.01f,
            true,
            true,
            nullptr,
            "source motion 방향에 곱해지는 spawn 시점 초기 속도 크기입니다."
        );
    }

    return changed;
}

bool AccelerationModuleDetail::Draw(const AuthoringEmitter& emitter, AuthoringModule&, AccelerationModuleData& data, DetailPropertyContext& detailContext)
{
    const AccelerationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("가속", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("AccelerationSpace"))
        {
            changed |= Draw_AccelerationTimeBasisProperty(
                detailContext,
                data.timeBasis,
                defaultData.timeBasis
            );
            changed |= detailContext.Draw_BoolProperty(
                "월드 스페이스",
                data.inWorldSpace,
                defaultData.inWorldSpace,
                "켜면 가속 벡터를 world 기준으로, 끄면 emitter local 기준으로 해석합니다. 중력/상승처럼 effect 회전과 무관한 힘은 켜둡니다."
            );
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup(
            "가속",
            data.acceleration,
            defaultData.acceleration,
            0.01f,
            true,
            false,
            nullptr,
            "매 frame 현재 속도에 누적되는 가속도 벡터입니다."
        );
        if (emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
            ImGui::TextDisabled("Sprite Trail에서 Path Follow/Replay가 켜지면 path module이 stamp 중심 위치를 소유합니다.");
    }

    return changed;
}

bool DragModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, DragModuleData& data, DetailPropertyContext& detailContext)
{
    const DragModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("드래그", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "드래그",
            data.drag,
            defaultData.drag,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "drag"),
            "현재 속도를 감쇠시키는 저항 계수입니다. 값이 클수록 더 빨리 느려집니다."
        );
    }

    return changed;
}

bool VelocityOverLifeModuleDetail::Draw(const AuthoringEmitter& emitter, const AuthoringModule& module, VelocityOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const VelocityOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "속도 배율",
            data.scaleOverLife,
            defaultData.scaleOverLife,
            0.01f,
            true,
            true,
            nullptr,
            "생존 중 normalized life(0->1) 기준으로 현재 속도에 곱하는 배율입니다."
        );

        if (ImGui::CollapsingHeader("고급 적용 대상"))
        {
            if (detailContext.Begin_PropertyTable("VelocityOverLifeApplyChannels"))
            {
                changed |= Draw_VelocityOverLifeApplyChannelMaskProperty(
                    detailContext,
                    data.applyChannelMask,
                    defaultData.applyChannelMask
                );
                detailContext.End_PropertyTable();
            }
        }

        if (Has_VelocityOverLifeChannelConflict(emitter, module, data))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 1.f, 0.78f, 0.28f, 1.f });
            ImGui::TextWrapped("겹친 대상은 위쪽 VelocityOverLife가 우선 적용됩니다.");
            ImGui::PopStyleColor();
        }
    }

    return changed;
}

bool InitialRotationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialRotationModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialRotationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "회전각",
            data.rotationDegrees,
            defaultData.rotationDegrees,
            0.01f,
            true,
            true,
            nullptr,
            "spawn 시점에 한 번 샘플되는 초기 회전각입니다. 단위는 degree입니다."
        );
    }

    return changed;
}

bool OrbitOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, OrbitOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const OrbitOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 공전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("OrbitOverLife"))
        {
            Draw_ReadOnlyTextProperty(detailContext, "Pivot", "Emitter Origin", "v1 runtime은 EmitterOrigin만 소비합니다.");
            changed |= Draw_EffectOrbitPlaneProperty(detailContext, "평면", data.plane, defaultData.plane);
            Draw_ReadOnlyTextProperty(detailContext, "방향 추적", "Preserve", "v1 runtime은 orientation follow를 적용하지 않습니다.");
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "공전 각도",
            data.angleDegreesOverLife,
            defaultData.angleDegreesOverLife,
            0.01f,
            true,
            true,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 emitter origin 주변에 추가로 공전시키는 각도입니다."
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "반지름 배율",
            data.radiusScaleOverLife,
            defaultData.radiusScaleOverLife,
            0.01f,
            true,
            true,
            nullptr,
            "생존 중 기존 offset 반지름에 곱하는 배율입니다."
        );
    }

    return changed;
}

bool PlaneRadialOrientationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    PlaneRadialOrientationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const PlaneRadialOrientationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("평면/방사 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("PlaneRadialOrientation"))
        {
            Draw_PlaneRadialOrientationResolvedTargetProperty(detailContext, emitter);
            changed |= Draw_PlaneRadialOrientationModeProperty(
                detailContext,
                "방향",
                data.orientationMode,
                defaultData.orientationMode
            );

            if (Should_ShowPlaneRadialOrientationMeshFields(emitter))
            {
                changed |= Draw_PlaneRadialOrientationAxisProperty(
                    detailContext,
                    "메시 앞축",
                    data.meshForwardAxis,
                    defaultData.meshForwardAxis
                );
                changed |= Draw_PlaneRadialOrientationAxisProperty(
                    detailContext,
                    "메시 위축",
                    data.meshUpAxis,
                    defaultData.meshUpAxis
                );
                changed |= detailContext.Draw_FloatProperty(
                    "기울기",
                    data.tiltDegrees,
                    defaultData.tiltDegrees,
                    0.1f,
                    "radial orientation 결과에 더하는 pitch/tilt 보정입니다. 단위는 degree입니다."
                );
                changed |= detailContext.Draw_FloatProperty(
                    "Roll 오프셋",
                    data.rollOffsetDegrees,
                    defaultData.rollOffsetDegrees,
                    0.1f,
                    "radial orientation 결과에 더하는 roll 보정입니다. 단위는 degree입니다."
                );
            }

            detailContext.End_PropertyTable();
        }

        if (!Has_PlaneRadialLocationModule(emitter))
            ImGui::TextDisabled("같은 emitter에 평면/방사 위치가 없으면 runtime radial frame을 만들 수 없습니다.");
    }

    return changed;
}

bool CylinderOrientationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    CylinderOrientationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const CylinderOrientationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("실린더 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("CylinderOrientation"))
        {
            Draw_ReadOnlyTextProperty(
                detailContext,
                "대상",
                Get_PlaneRadialOrientationTargetKindLabel(Resolve_PlaneRadialOrientationTargetKind(emitter)),
                "현재 emitter 타입 기준으로 자동 결정됩니다. targetKind 저장값은 유지하지만 이번 단계에서는 편집 UI를 숨깁니다."
            );
            changed |= Draw_CylinderOrientationModeProperty(
                detailContext,
                "방향",
                data.orientationMode,
                defaultData.orientationMode
            );

            if (!Is_MeshEffectAuthoring(emitter))
            {
                changed |= detailContext.Draw_BoolProperty(
                    "공전 방향 따라 갱신",
                    data.followOrbitOverLife,
                    defaultData.followOrbitOverLife,
                    "OrbitOverLife가 위치를 돌릴 때 sprite가 바라보는 cylinder 방향도 같은 회전으로 갱신합니다."
                );
            }
            else
            {
                changed |= Draw_PlaneRadialOrientationAxisProperty(
                    detailContext,
                    "메시 앞축",
                    data.meshForwardAxis,
                    defaultData.meshForwardAxis
                );
                changed |= Draw_PlaneRadialOrientationAxisProperty(
                    detailContext,
                    "메시 위축",
                    data.meshUpAxis,
                    defaultData.meshUpAxis
                );
                changed |= detailContext.Draw_FloatProperty(
                    "기울기",
                    data.tiltDegrees,
                    defaultData.tiltDegrees,
                    0.1f,
                    "cylinder orientation 결과에 더하는 pitch/tilt 보정입니다. 단위는 degree입니다."
                );
                changed |= detailContext.Draw_FloatProperty(
                    "Roll 오프셋",
                    data.rollOffsetDegrees,
                    defaultData.rollOffsetDegrees,
                    0.1f,
                    "cylinder orientation 결과에 더하는 roll 보정입니다. 단위는 degree입니다."
                );
            }

            detailContext.End_PropertyTable();
        }

        if (!Has_CylinderLocationModule(emitter))
            ImGui::TextDisabled("같은 emitter에 실린더 위치가 없으면 runtime cylinder frame을 만들 수 없습니다.");
        if (data.orientationMode != CylinderOrientationMode::None &&
            Has_EnabledPlaneRadialOrientationModule(emitter))
            ImGui::TextDisabled("실린더 회전이 켜져 있으면 runtime은 평면/방사 회전보다 실린더 회전을 먼저 적용합니다.");
    }

    return changed;
}

bool SphereRadialOrientationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SphereRadialOrientationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SphereRadialOrientationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("구/방사 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SphereRadialOrientation"))
        {
            changed |= Draw_SphereRadialOrientationModeProperty(
                detailContext,
                "방향",
                data.orientationMode,
                defaultData.orientationMode
            );
            changed |= Draw_PlaneRadialOrientationAxisProperty(
                detailContext,
                "메시 앞축",
                data.meshForwardAxis,
                defaultData.meshForwardAxis
            );
            changed |= Draw_PlaneRadialOrientationAxisProperty(
                detailContext,
                "메시 위축",
                data.meshUpAxis,
                defaultData.meshUpAxis
            );
            changed |= detailContext.Draw_FloatProperty(
                "기울기",
                data.tiltDegrees,
                defaultData.tiltDegrees,
                0.1f,
                "sphere radial orientation 결과에 더하는 pitch/tilt 보정입니다. 단위는 degree입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "Roll 오프셋",
                data.rollOffsetDegrees,
                defaultData.rollOffsetDegrees,
                0.1f,
                "sphere radial orientation 결과에 더하는 roll 보정입니다. 단위는 degree입니다."
            );

            detailContext.End_PropertyTable();
        }

        if (!Has_SphereLocationModule(emitter))
            ImGui::TextDisabled("같은 emitter에 구 위치가 없으면 runtime sphere radial frame을 만들 수 없습니다.");
        if (data.orientationMode != SphereRadialOrientationMode::None &&
            Has_EnabledPlaneRadialOrientationModule(emitter))
            ImGui::TextDisabled("평면/방사 회전도 켜져 있으면 runtime은 구/방사 회전을 먼저 적용합니다.");
    }

    return changed;
}

bool RotationOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, RotationOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const RotationOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "추가 회전각",
            data.rotationOverLife,
            defaultData.rotationOverLife,
            0.01f,
            true,
            true,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 초기 회전 위에 더하는 추가 회전각입니다."
        );
    }

    return changed;
}

bool SpriteTiltModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SpriteTiltModuleData& data, DetailPropertyContext& detailContext)
{
    const SpriteTiltModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("스프라이트 틸트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector2DistributionGroup(
            "틸트 (도)",
            data.tiltDegrees,
            defaultData.tiltDegrees,
            0.01f,
            true,
            nullptr,
            "spawn/stamp 생성 시점에 카드 면을 local X/Y 방향으로 기울입니다. Rotation은 카드 면 안 roll 회전입니다."
        );
    }

    return changed;
}

bool SpriteTiltOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SpriteTiltOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const SpriteTiltOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 스프라이트 틸트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector2DistributionGroup(
            "추가 틸트 (도)",
            data.tiltOverLife,
            defaultData.tiltOverLife,
            0.01f,
            true,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 카드 면 기울기를 추가합니다. RotationOverLife는 카드 면 안 roll 회전입니다."
        );
    }

    return changed;
}

bool InitialRotationRateModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialRotationRateModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialRotationRateModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "회전 속도",
            data.rotationRateDegrees,
            defaultData.rotationRateDegrees,
            0.01f,
            true,
            true,
            nullptr,
            "spawn 시점에 한 번 샘플되는 초당 회전 속도입니다. 단위는 degree/sec입니다."
        );
    }

    return changed;
}

bool RotationRateScaleByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    RotationRateScaleByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const RotationRateScaleByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "회전 속도 배율",
            data.scaleOverLife,
            defaultData.scaleOverLife,
            0.01f,
            true,
            true,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 초기 회전 속도에 곱하는 배율입니다."
        );
    }

    return changed;
}

bool InitialMeshRotationModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialMeshRotationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialMeshRotationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 메시 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector3DistributionGroup(
            "회전 XYZ",
            data.rotationDegrees,
            defaultData.rotationDegrees,
            0.01f,
            true,
            false,
            nullptr,
            "spawn 시점에 한 번 샘플되는 mesh 초기 회전입니다. 단위는 degree입니다."
        );
    }

    return changed;
}

bool MeshRotationOverLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshRotationOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshRotationOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector3DistributionGroup(
            "추가 회전 XYZ",
            data.rotationOverLife,
            defaultData.rotationOverLife,
            0.01f,
            true,
            false,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 mesh 초기 회전 위에 더하는 값입니다."
        );
    }

    return changed;
}

bool MeshDirectionAlignOverLifeModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    MeshDirectionAlignOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshDirectionAlignOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 방향 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("MeshDirectionAlignOverLife"))
        {
            changed |= Draw_MeshDirectionAlignTargetModeProperty(detailContext, data.targetMode, defaultData.targetMode);
            changed |= Draw_MeshDirectionAlignSpaceProperty(detailContext, data.space, defaultData.space);
            changed |= detailContext.Draw_Vec3Property(
                data.targetMode == MeshDirectionAlignTargetMode::Point ? "대상 점" : "대상 방향",
                data.target,
                defaultData.target,
                0.01f,
                nullptr,
                data.targetMode == MeshDirectionAlignTargetMode::Point
                ? "Point 방식에서는 이 값을 target 위치로 해석합니다."
                : "Direction 방식에서는 이 값을 바라볼 방향 벡터로 해석합니다."
            );
            changed |= Draw_PlaneRadialOrientationAxisProperty(
                detailContext,
                "메시 앞축",
                data.meshForwardAxis,
                defaultData.meshForwardAxis
            );
            changed |= Draw_PlaneRadialOrientationAxisProperty(
                detailContext,
                "메시 위축",
                data.meshUpAxis,
                defaultData.meshUpAxis
            );
            changed |= Draw_MeshDirectionAlignBlendModeProperty(detailContext, data.blendMode, defaultData.blendMode);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "정렬 진행도",
            data.alignmentProgress,
            defaultData.alignmentProgress,
            0.01f,
            false,
            true,
            nullptr,
            "normalized life 기준 target orientation으로 붙는 0..1 weight입니다."
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "랜덤 지연",
            data.randomDelay,
            defaultData.randomDelay,
            0.01f,
            true,
            false,
            nullptr,
            "spawn 시점에 particle별로 한 번 샘플되는 정렬 시작 지연입니다. 0..1 범위로 clamp됩니다."
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "랜덤 배율",
            data.randomWeightScale,
            defaultData.randomWeightScale,
            0.01f,
            true,
            false,
            nullptr,
            "spawn 시점에 particle별로 한 번 샘플되는 최종 weight 배율입니다. 0..1 범위로 clamp됩니다."
        );

        ImGui::TextDisabled("모든 메시 회전 계산 뒤에 적용됩니다. 최종 weight 1에서는 목표 방향이 최종 회전을 결정합니다.");
        if (Has_EnabledModule(emitter, AuthoringModuleType::MeshRotationOverLife) ||
            Has_EnabledModule(emitter, AuthoringModuleType::InitialMeshRotationRate) ||
            Has_EnabledModule(emitter, AuthoringModuleType::MeshRotationRateScaleByLife))
            ImGui::TextDisabled("기존 메시 회전 모듈은 weight가 낮은 구간에서 함께 보이고, weight가 높아질수록 target 방향에 흡수됩니다.");
    }

    return changed;
}

bool InitialMeshRotationRateModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialMeshRotationRateModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialMeshRotationRateModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 메시 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("InitialMeshRotationRate"))
        {
            changed |= detailContext.Draw_BoolProperty(
                "월드 스페이스",
                data.inWorldSpace,
                defaultData.inWorldSpace,
                "켜면 회전 속도 축을 world 기준으로 해석합니다."
            );
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup(
            "회전 속도 XYZ",
            data.rotationRateDegrees,
            defaultData.rotationRateDegrees,
            0.01f,
            true,
            false,
            nullptr,
            "spawn 시점에 한 번 샘플되는 mesh 축별 초당 회전 속도입니다."
        );
    }

    return changed;
}

bool MeshRotationRateScaleByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshRotationRateScaleByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshRotationRateScaleByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_Vector3DistributionGroup(
            "회전 속도 배율 XYZ",
            data.scaleOverLife,
            defaultData.scaleOverLife,
            0.01f,
            true,
            false,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 mesh 회전 속도에 곱하는 축별 배율입니다."
        );
    }

    return changed;
}

bool InitialColorModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, InitialColorModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialColorModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 컬러", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_ColorRgbDistributionGroup("색상(RGB)", data.color, defaultData.color);
        changed |= detailContext.Draw_FloatDistributionGroup(
            "알파",
            data.alpha,
            defaultData.alpha,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "alpha"),
            "spawn 시점에 한 번 샘플되는 초기 alpha입니다. material opacity에 곱해집니다."
        );
    }

    return changed;
}

bool ColorOverLifeModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, ColorOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const ColorOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_ColorRgbDistributionGroup("색상(RGB)", data.colorOverLife, defaultData.colorOverLife);
        changed |= detailContext.Draw_FloatDistributionGroup(
            "알파",
            data.alphaOverLife,
            defaultData.alphaOverLife,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "alphaOverLife"),
            "생존 중 normalized life(0->1)에 따라 material opacity에 곱해지는 alpha 배율입니다."
        );
    }

    return changed;
}

bool SubUVFrameOverLifeModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SubUVFrameOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SubUVFrameOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("서브UV 프레임 선택/재생", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SubUVFrameOverLife"))
        {
            changed |= Draw_SubUVFramePlaybackModeProperty(
                detailContext,
                "재생 방식",
                data.playbackMode,
                defaultData.playbackMode
            );

            if (data.playbackMode == SubUVFramePlaybackMode::FixedFrame)
                changed |= detailContext.Draw_UintProperty("프레임", data.startFrame, defaultData.startFrame, "고정 재생에서 사용할 atlas frame index입니다.");
            else
            {
                changed |= detailContext.Draw_UintProperty(
                    "시작 프레임",
                    data.startFrame,
                    defaultData.startFrame,
                    "재생 range의 시작 frame입니다. 시작과 종료가 같으면 단일 frame으로 유지됩니다."
                );
                changed |= detailContext.Draw_UintProperty(
                    "종료 프레임",
                    data.endFrame,
                    defaultData.endFrame,
                    "재생 range의 끝 frame입니다. 시작보다 작으면 atlas 끝을 거쳐 wrap 재생합니다."
                );

                if (data.playbackMode == SubUVFramePlaybackMode::FramesPerSecond)
                {
                    changed |= detailContext.Draw_BoolProperty("루프", data.loop, defaultData.loop, "켜면 start/end로 정한 range 안에서 반복 재생합니다.");
                    changed |= detailContext.Draw_FloatProperty(
                        "FPS",
                        data.framesPerSecond,
                        defaultData.framesPerSecond,
                        0.1f,
                        "초당 넘길 atlas frame 수입니다. LifeProgress 모드와 달리 시간 기반으로 진행합니다."
                    );
                    changed |= detailContext.Draw_BoolProperty(
                        "개체별 랜덤 시작",
                        data.randomStartPhase,
                        defaultData.randomStartPhase,
                        "particle/stamp/strip별로 SubUV range 안의 시작 phase를 다르게 합니다."
                    );
                }
            }

            detailContext.End_PropertyTable();
        }

        const bool usesRandomSeed =
            data.playbackMode == SubUVFramePlaybackMode::RandomFrame ||
            (data.playbackMode == SubUVFramePlaybackMode::FramesPerSecond && data.randomStartPhase);

        if (usesRandomSeed)
            changed |= Draw_RandomSeedProperty(detailContext, data.randomSeed);

        const uint32 frameCount = Resolve_SubUvFrameCount(emitter);
        const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
        data.startFrame = min(data.startFrame, lastFrame);
        data.endFrame = min(data.endFrame, lastFrame);
        data.framesPerSecond = max(0.f, data.framesPerSecond);
        changed |= Normalize_SubUVFrameIndexDistribution(data.frameIndex, lastFrame);

        ImGui::TextDisabled("현재 atlas frame 수: %u", frameCount);
    }

    return changed;
}

bool SizeByLifeModuleDetail::Draw(
    AuthoringEmitter& emitter,
    AuthoringModule&,
    SizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SizeByLife"))
        {
            changed |= detailContext.Draw_BoolProperty("X축 적용", data.multiplyX, defaultData.multiplyX, "켜면 life 기반 크기 배율을 X축에 적용합니다.");
            changed |= detailContext.Draw_BoolProperty("Y축 적용", data.multiplyY, defaultData.multiplyY, "켜면 life 기반 크기 배율을 Y축에 적용합니다.");
            detailContext.End_PropertyTable();
        }

        const auto axis_lock_label = [](SizeByLifeModuleData::AxisLock axisLock)
        {
            switch (axisLock)
            {
            case SizeByLifeModuleData::AxisLock::X:
                return "X";
            case SizeByLifeModuleData::AxisLock::Y:
                return "Y";
            case SizeByLifeModuleData::AxisLock::None:
            default:
                return "None";
            }
        };

        ImGui::PushID("SizeByLifeAxisLock");
        ImGui::TextUnformatted("축 잠금");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("한 축의 life 배율을 다른 축에도 복사해 종횡비를 유지합니다. None이면 축별 값을 따로 사용합니다.");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("##AxisLock", axis_lock_label(data.axisLock)))
        {
            const auto draw_axis_lock = [&](SizeByLifeModuleData::AxisLock value)
            {
                const bool selected = data.axisLock == value;
                if (ImGui::Selectable(axis_lock_label(value), selected))
                {
                    data.axisLock = value;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            };

            draw_axis_lock(SizeByLifeModuleData::AxisLock::None);
            draw_axis_lock(SizeByLifeModuleData::AxisLock::X);
            draw_axis_lock(SizeByLifeModuleData::AxisLock::Y);
            ImGui::EndCombo();
        }
        ImGui::PopID();

        changed |= detailContext.Draw_Vector2DistributionGroup(
            "수명 배율",
            data.scaleOverLife,
            defaultData.scaleOverLife,
            0.01f,
            true,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 초기 크기에 곱하는 배율입니다."
        );
    }

    return changed;
}

bool BeamEnvelopeOverLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    BeamEnvelopeOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const BeamEnvelopeOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("시작점 쪽 경계", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushID("StartRatioOverLife");
        changed |= detailContext.Draw_FloatDistributionGroup(
            "위치 비율",
            data.startRatioOverLife,
            defaultData.startRatioOverLife,
            0.01f,
            false,
            true,
            nullptr,
            "0은 Beam 시작점, 1은 Beam 끝점입니다."
        );
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("끝점 쪽 경계", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushID("EndRatioOverLife");
        changed |= detailContext.Draw_FloatDistributionGroup(
            "위치 비율",
            data.endRatioOverLife,
            defaultData.endRatioOverLife,
            0.01f,
            false,
            true,
            nullptr,
            "0은 Beam 시작점, 1은 Beam 끝점입니다."
        );
        ImGui::PopID();
    }

    if (ImGui::CollapsingHeader("폭", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushID("WidthScaleOverLife");
        changed |= detailContext.Draw_FloatDistributionGroup(
            "폭 배율",
            data.widthScaleOverLife,
            defaultData.widthScaleOverLife,
            0.01f,
            false,
            true,
            nullptr,
            "SizeByLife 뒤에 추가로 곱하는 Beam 폭 배율입니다."
        );
        ImGui::PopID();
    }

    ImGui::TextDisabled("두 경계가 교차하면 작은 값부터 큰 값까지의 구간을 표시합니다.");

    return changed;
}

bool MeshSizeByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshSizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshSizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("MeshSizeByLife"))
        {
            changed |= detailContext.Draw_BoolProperty("X축 적용", data.multiplyX, defaultData.multiplyX, "켜면 life 기반 mesh 크기 배율을 X축에 적용합니다.");
            changed |= detailContext.Draw_BoolProperty("Y축 적용", data.multiplyY, defaultData.multiplyY, "켜면 life 기반 mesh 크기 배율을 Y축에 적용합니다.");
            changed |= detailContext.Draw_BoolProperty("Z축 적용", data.multiplyZ, defaultData.multiplyZ, "켜면 life 기반 mesh 크기 배율을 Z축에 적용합니다.");
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup(
            "수명 배율 XYZ",
            data.scaleOverLife,
            defaultData.scaleOverLife,
            0.01f,
            true,
            false,
            nullptr,
            "생존 중 normalized life(0->1)에 따라 mesh 초기 크기에 곱하는 축별 배율입니다."
        );
    }

    return changed;
}

bool SpawnPerUnitModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SpawnPerUnitModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SpawnPerUnitModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("거리당 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "분포",
            data.spawnPerUnit,
            defaultData.spawnPerUnit,
            0.01f,
            true,
            true,
            nullptr,
            "provider 이동 거리당 몇 개를 만들지 정하는 spawn 밀도입니다."
        );

        if (detailContext.Begin_PropertyTable("SpawnPerUnitPolicy"))
        {
            changed |= detailContext.Draw_FloatProperty(
                "단위 스칼라",
                data.unitScalar,
                defaultData.unitScalar,
                0.01f,
                "분포 값을 실제 world distance로 환산하는 기준 스칼라입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "무브먼트 허용치",
                data.movementTolerance,
                defaultData.movementTolerance,
                0.001f,
                "이 거리 미만의 provider sample 이동은 새 history sample로 받지 않습니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "최대 프레임 거리",
                data.maxFrameDistance,
                defaultData.maxFrameDistance,
                0.01f,
                "한 frame 이동이 이 거리보다 크면 teleport/skip으로 보고 history를 재시드합니다. 0 이하면 제한이 없습니다."
            );
            detailContext.End_PropertyTable();
        }
    }

    data.unitScalar = max(0.0001f, data.unitScalar);
    data.movementTolerance = max(0.f, data.movementTolerance);
    data.maxFrameDistance = max(0.f, data.maxFrameDistance);
    return changed;
}

bool SourceHistorySpriteTrailPathFollowModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SourceHistorySpriteTrailPathFollowModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SourceHistorySpriteTrailPathFollowModuleData defaultData{};
    bool changed = false;

    const auto direction_label = [](SourceHistorySpriteTrailPathFollowDirection direction)
    {
        switch (direction)
        {
        case SourceHistorySpriteTrailPathFollowDirection::TowardHead:
            return "Head 방향";
        case SourceHistorySpriteTrailPathFollowDirection::TowardTail:
            return "Tail 방향";
        default:
            return "Head 방향";
        }
    };

    const auto arrival_mode_label = [](SourceHistorySpriteTrailArrivalMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailArrivalMode::ClampAtEnd:
            return "끝점 고정";
        case SourceHistorySpriteTrailArrivalMode::KillOnArrive:
        default:
            return "도착 시 제거";
        }
    };

    if (ImGui::CollapsingHeader("경로 따라가기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SourceHistorySpriteTrailPathFollowPolicy"))
        {
            ImGui::PushID("Direction");
            detailContext.Draw_PropertyLabel("방향");
            SourceHistorySpriteTrailPathFollowDirection nextDirection = data.direction;
            if (ImGui::BeginCombo("##Value", direction_label(nextDirection)))
            {
                const SourceHistorySpriteTrailPathFollowDirection candidates[] = {
                    SourceHistorySpriteTrailPathFollowDirection::TowardHead,
                    SourceHistorySpriteTrailPathFollowDirection::TowardTail,
                };
                for (const SourceHistorySpriteTrailPathFollowDirection candidate : candidates)
                {
                    const bool selected = nextDirection == candidate;
                    if (ImGui::Selectable(direction_label(candidate), selected))
                        nextDirection = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.direction != nextDirection)
            {
                data.direction = nextDirection;
                changed = true;
            }
            ImGui::PopID();

            ImGui::PushID("ArrivalMode");
            detailContext.Draw_PropertyLabel("도착 후 처리");
            SourceHistorySpriteTrailArrivalMode nextArrivalMode = data.arrivalMode;
            if (ImGui::BeginCombo("##Value", arrival_mode_label(nextArrivalMode)))
            {
                const SourceHistorySpriteTrailArrivalMode candidates[] = {
                    SourceHistorySpriteTrailArrivalMode::KillOnArrive,
                    SourceHistorySpriteTrailArrivalMode::ClampAtEnd,
                };
                for (const SourceHistorySpriteTrailArrivalMode candidate : candidates)
                {
                    const bool selected = nextArrivalMode == candidate;
                    if (ImGui::Selectable(arrival_mode_label(candidate), selected))
                        nextArrivalMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.arrivalMode != nextArrivalMode)
            {
                data.arrivalMode = nextArrivalMode;
                changed = true;
            }
            ImGui::PopID();
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
        changed |= detailContext.Draw_FloatDistributionGroup("시작 딜레이", data.startDelay, defaultData.startDelay);
    }

    return changed;
}

bool SourceHistorySpriteTrailPathReplayModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SourceHistorySpriteTrailPathReplayModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SourceHistorySpriteTrailPathReplayModuleData defaultData{};
    bool changed = false;

    const auto replay_mode_label = [](SourceHistorySpriteTrailPathReplayMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailPathReplayMode::RecordedSpeed:
            return "기록 속도";
        case SourceHistorySpriteTrailPathReplayMode::FitDuration:
            return "지정 시간";
        default:
            return "기록 속도";
        }
    };

    const auto start_mode_label = [](SourceHistorySpriteTrailPathReplayStartMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailPathReplayStartMode::TailFirst:
            return "Tail 먼저";
        case SourceHistorySpriteTrailPathReplayStartMode::AllAtOnce:
            return "동시 시작";
        default:
            return "Tail 먼저";
        }
    };

    const auto arrival_mode_label = [](SourceHistorySpriteTrailArrivalMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailArrivalMode::ClampAtEnd:
            return "끝점 고정";
        case SourceHistorySpriteTrailArrivalMode::KillOnArrive:
        default:
            return "도착 시 제거";
        }
    };

    if (ImGui::CollapsingHeader("경로 리플레이", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SourceHistorySpriteTrailPathReplayPolicy"))
        {
            changed |= detailContext.Draw_FloatProperty(
                "딜레이",
                data.delayTime,
                defaultData.delayTime,
                0.01f,
                "stamp 생성 후 replay 시작 전 유지 시간입니다."
            );

            ImGui::PushID("ReplayMode");
            detailContext.Draw_PropertyLabel("재생 방식");
            SourceHistorySpriteTrailPathReplayMode nextReplayMode = data.replayMode;
            if (ImGui::BeginCombo("##Value", replay_mode_label(nextReplayMode)))
            {
                const SourceHistorySpriteTrailPathReplayMode candidates[] = {
                    SourceHistorySpriteTrailPathReplayMode::RecordedSpeed,
                    SourceHistorySpriteTrailPathReplayMode::FitDuration,
                };
                for (const SourceHistorySpriteTrailPathReplayMode candidate : candidates)
                {
                    const bool selected = nextReplayMode == candidate;
                    if (ImGui::Selectable(replay_mode_label(candidate), selected))
                        nextReplayMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.replayMode != nextReplayMode)
            {
                data.replayMode = nextReplayMode;
                changed = true;
            }
            ImGui::PopID();

            changed |= detailContext.Draw_FloatProperty(
                "속도 배율",
                data.speedScale,
                defaultData.speedScale,
                0.01f,
                "기록 속도 모드에서 source 기록 속도에 곱하는 배율입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "흡수 시간",
                data.drainDuration,
                defaultData.drainDuration,
                0.01f,
                "지정 시간 모드에서 head까지 따라붙는 총 시간입니다."
            );

            ImGui::PushID("StartMode");
            detailContext.Draw_PropertyLabel("시작 방식");
            SourceHistorySpriteTrailPathReplayStartMode nextStartMode = data.startMode;
            if (ImGui::BeginCombo("##Value", start_mode_label(nextStartMode)))
            {
                const SourceHistorySpriteTrailPathReplayStartMode candidates[] = {
                    SourceHistorySpriteTrailPathReplayStartMode::TailFirst,
                    SourceHistorySpriteTrailPathReplayStartMode::AllAtOnce,
                };
                for (const SourceHistorySpriteTrailPathReplayStartMode candidate : candidates)
                {
                    const bool selected = nextStartMode == candidate;
                    if (ImGui::Selectable(start_mode_label(candidate), selected))
                        nextStartMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.startMode != nextStartMode)
            {
                data.startMode = nextStartMode;
                changed = true;
            }
            ImGui::PopID();

            ImGui::PushID("ArrivalMode");
            detailContext.Draw_PropertyLabel("도착 후 처리");
            SourceHistorySpriteTrailArrivalMode nextArrivalMode = data.arrivalMode;
            if (ImGui::BeginCombo("##Value", arrival_mode_label(nextArrivalMode)))
            {
                const SourceHistorySpriteTrailArrivalMode candidates[] = {
                    SourceHistorySpriteTrailArrivalMode::KillOnArrive,
                    SourceHistorySpriteTrailArrivalMode::ClampAtEnd,
                };
                for (const SourceHistorySpriteTrailArrivalMode candidate : candidates)
                {
                    const bool selected = nextArrivalMode == candidate;
                    if (ImGui::Selectable(arrival_mode_label(candidate), selected))
                        nextArrivalMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.arrivalMode != nextArrivalMode)
            {
                data.arrivalMode = nextArrivalMode;
                changed = true;
            }
            ImGui::PopID();
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("흡수 곡선", data.drainCurve, defaultData.drainCurve, 0.01f, false, true);
    }

    data.delayTime = max(0.f, data.delayTime);
    data.speedScale = max(0.f, data.speedScale);
    data.drainDuration = max(0.0001f, data.drainDuration);
    return changed;
}

bool MaterialScalarModulationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    MaterialScalarModulationModuleData& data,
    DetailPropertyContext& detailContext)
{
    bool changed = false;
    const bool allowScalarParticleLife = Is_MaterialScalarParticleLifeSupported(emitter);

    if (ImGui::CollapsingHeader("머티리얼 변조", ImGuiTreeNodeFlags_DefaultOpen))
    {
        constexpr uint32 kMaxMaterialScalarModulators{ 8 };
        constexpr uint32 kMaxCoreColorRgbModulators{ 8 };
        constexpr uint32 kMaxMaterialVec2Modulators{ 8 };
        if (ImGui::Button("+ 변조 추가"))
        {
            data.modulators.push_back(MaterialScalarModulatorData{});
            changed = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("material instance parameter를 시간 축에 따라 변조하는 row를 추가합니다. 대상은 row 내부 콤보에서 고릅니다.");

        ImGui::SameLine();
        ImGui::TextDisabled(
            "%zu / %u",
            data.modulators.size() + data.coreColorRgbModulators.size() + data.vec2Modulators.size(),
            kMaxMaterialScalarModulators + kMaxCoreColorRgbModulators + kMaxMaterialVec2Modulators
        );

        enum class MaterialModulationUiRowKind : uint8
        {
            Scalar,
            CoreColorRgb,
            Vec2,
        };

        struct MaterialModulationUiRow
        {
            MaterialModulationUiRowKind kind{ MaterialModulationUiRowKind::Scalar };
            size_t index{};
        };

        vector<MaterialModulationUiRow> rows{};
        rows.reserve(data.modulators.size() + data.coreColorRgbModulators.size() + data.vec2Modulators.size());
        for (size_t index = 0; index < data.modulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ MaterialModulationUiRowKind::Scalar, index });
        for (size_t index = 0; index < data.coreColorRgbModulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ MaterialModulationUiRowKind::CoreColorRgb, index });
        for (size_t index = 0; index < data.vec2Modulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ MaterialModulationUiRowKind::Vec2, index });

        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            const MaterialModulationUiRow row = rows[rowIndex];
            ImGui::PushID(
                row.kind == MaterialModulationUiRowKind::CoreColorRgb ? "CoreColorRgb" : row.kind == MaterialModulationUiRowKind::Vec2 ? "Vec2" : "Scalar"
            );
            ImGui::PushID(static_cast<int>(row.index));

            const MaterialScalarModulationTimeSource timeSource =
                row.kind == MaterialModulationUiRowKind::CoreColorRgb
                ? data.coreColorRgbModulators[row.index].timeSource
                : row.kind == MaterialModulationUiRowKind::Vec2
                  ? data.vec2Modulators[row.index].timeSource
                  : data.modulators[row.index].timeSource;
            const string targetLabel =
                row.kind == MaterialModulationUiRowKind::Vec2
                ? Get_MaterialVec2TargetLabel(data.vec2Modulators[row.index].targetField)
                : Get_MaterialModulationTargetLabel(
                    row.kind == MaterialModulationUiRowKind::CoreColorRgb,
                    row.kind == MaterialModulationUiRowKind::CoreColorRgb
                    ? MaterialScalarModulationTargetField::CoreColor
                    : data.modulators[row.index].targetField
                );
            const string header =
                string("#") + to_string(rowIndex + 1) + " " +
                targetLabel + " / " +
                Get_MaterialScalarTimeSourceLabel(timeSource);
            bool structureChanged = false;

            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (row.kind == MaterialModulationUiRowKind::CoreColorRgb)
                {
                    MaterialCoreColorRgbModulatorData& modulator = data.coreColorRgbModulators[row.index];
                    MaterialCoreColorRgbModulatorData defaultData = Make_CoreColorRgbModulator(emitter);

                    if (detailContext.Begin_PropertyTable("MaterialCoreColorRgbModulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty(
                            "변조 사용",
                            modulator.enabled,
                            defaultData.enabled,
                            "이 변조 row를 runtime preview/export에 반영할지 정합니다."
                        );

                        MaterialModulationTargetSelection currentSelection{};
                        currentSelection.category = MaterialModulationTargetCategory::CoreColorRgb;
                        currentSelection.scalarTarget = MaterialScalarModulationTargetField::CoreColor;
                        MaterialModulationTargetSelection nextSelection = currentSelection;
                        if (Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            currentSelection,
                            nextSelection
                        ))
                        {
                            changed = true;
                            if (nextSelection.category == MaterialModulationTargetCategory::Scalar)
                            {
                                MaterialScalarModulatorData scalarModulator{};
                                scalarModulator.enabled = modulator.enabled;
                                scalarModulator.timeSource = modulator.timeSource;
                                scalarModulator.targetField = nextSelection.scalarTarget;
                                if (Is_MaterialScrollSpeedScaleTarget(scalarModulator.targetField))
                                    scalarModulator.timeSource = MaterialScalarModulationTimeSource::EmitterTime;
                                scalarModulator.distribution = FloatDistributionData::Make_Constant(1.f);
                                data.modulators.push_back(scalarModulator);
                                data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else if (nextSelection.category == MaterialModulationTargetCategory::Vec2)
                            {
                                MaterialVec2ModulatorData vec2Modulator{};
                                vec2Modulator.enabled = modulator.enabled;
                                vec2Modulator.timeSource = MaterialScalarModulationTimeSource::EmitterTime;
                                vec2Modulator.targetField = nextSelection.vec2Target;
                                vec2Modulator.distribution = Vector2DistributionData::Make_Constant(Vec2{ 0.f, 0.f });
                                data.vec2Modulators.push_back(vec2Modulator);
                                data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                        }

                        if (!structureChanged)
                        {
                            changed |= Draw_MaterialScalarTimeSourceProperty(
                                detailContext,
                                "시간 축",
                                modulator.timeSource,
                                defaultData.timeSource,
                                allowScalarParticleLife
                            );
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_Vector3DistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution,
                            0.01f,
                            true,
                            false,
                            nullptr,
                            "선택한 시간 축을 입력으로 Core Color RGB에 적용할 RGB 색 값을 평가합니다."
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
                else if (row.kind == MaterialModulationUiRowKind::Vec2)
                {
                    MaterialVec2ModulatorData& modulator = data.vec2Modulators[row.index];
                    const MaterialVec2ModulatorData defaultData{};

                    if (detailContext.Begin_PropertyTable("MaterialVec2Modulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty(
                            "변조 사용",
                            modulator.enabled,
                            defaultData.enabled,
                            "이 Vec2 변조 row를 runtime preview/export에 반영할지 정합니다."
                        );
                        MaterialModulationTargetSelection currentSelection{};
                        currentSelection.category = MaterialModulationTargetCategory::Vec2;
                        currentSelection.vec2Target = modulator.targetField;
                        MaterialModulationTargetSelection nextSelection = currentSelection;
                        if (Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            currentSelection,
                            nextSelection
                        ))
                        {
                            changed = true;
                            if (nextSelection.category == MaterialModulationTargetCategory::Scalar)
                            {
                                MaterialScalarModulatorData scalarModulator{};
                                scalarModulator.enabled = modulator.enabled;
                                scalarModulator.timeSource = MaterialScalarModulationTimeSource::EmitterTime;
                                scalarModulator.targetField = nextSelection.scalarTarget;
                                scalarModulator.distribution = FloatDistributionData::Make_Constant(1.f);
                                data.modulators.push_back(scalarModulator);
                                data.vec2Modulators.erase(data.vec2Modulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else if (nextSelection.category == MaterialModulationTargetCategory::CoreColorRgb)
                            {
                                MaterialCoreColorRgbModulatorData coreColorRgbModulator = Make_CoreColorRgbModulator(emitter);
                                coreColorRgbModulator.enabled = modulator.enabled;
                                coreColorRgbModulator.timeSource = modulator.timeSource;
                                data.coreColorRgbModulators.push_back(coreColorRgbModulator);
                                data.vec2Modulators.erase(data.vec2Modulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else
                                modulator.targetField = nextSelection.vec2Target;
                        }
                        if (!structureChanged)
                        {
                            detailContext.Draw_PropertyLabel("연산", "현재 Vec2 modulation은 대상 UV offset에 분포 값을 더하는 Add 연산입니다.");
                            ImGui::TextUnformatted("Add");
                            detailContext.Draw_PropertyLabel("시간 축", "Vec2 material modulation은 EmitterTime만 지원합니다.");
                            ImGui::TextUnformatted(Get_MaterialScalarTimeSourceLabel(modulator.timeSource));
                            if (detailContext.Draw_ResetButton(modulator.timeSource != defaultData.timeSource))
                            {
                                modulator.timeSource = defaultData.timeSource;
                                changed = true;
                            }
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_Vector2DistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution,
                            0.01f,
                            true,
                            nullptr,
                            "선택한 시간 축을 입력으로 대상 material UV offset에 더할 X/Y 값을 평가합니다."
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.vec2Modulators.erase(data.vec2Modulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
                else
                {
                    MaterialScalarModulatorData& modulator = data.modulators[row.index];
                    const MaterialScalarModulatorData defaultData{};

                    if (detailContext.Begin_PropertyTable("MaterialScalarModulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty(
                            "변조 사용",
                            modulator.enabled,
                            defaultData.enabled,
                            "이 변조 row를 runtime preview/export에 반영할지 정합니다."
                        );

                        MaterialModulationTargetSelection currentSelection{};
                        currentSelection.category = MaterialModulationTargetCategory::Scalar;
                        currentSelection.scalarTarget = modulator.targetField;
                        MaterialModulationTargetSelection nextSelection = currentSelection;
                        if (Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            currentSelection,
                            nextSelection
                        ))
                        {
                            changed = true;
                            if (nextSelection.category == MaterialModulationTargetCategory::CoreColorRgb)
                            {
                                MaterialCoreColorRgbModulatorData coreColorRgbModulator = Make_CoreColorRgbModulator(emitter);
                                coreColorRgbModulator.enabled = modulator.enabled;
                                coreColorRgbModulator.timeSource = modulator.timeSource;
                                data.coreColorRgbModulators.push_back(coreColorRgbModulator);
                                data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else if (nextSelection.category == MaterialModulationTargetCategory::Vec2)
                            {
                                MaterialVec2ModulatorData vec2Modulator{};
                                vec2Modulator.enabled = modulator.enabled;
                                vec2Modulator.timeSource = MaterialScalarModulationTimeSource::EmitterTime;
                                vec2Modulator.targetField = nextSelection.vec2Target;
                                vec2Modulator.distribution = Vector2DistributionData::Make_Constant(Vec2{ 0.f, 0.f });
                                data.vec2Modulators.push_back(vec2Modulator);
                                data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else
                            {
                                modulator.targetField = nextSelection.scalarTarget;
                                if (Is_MaterialScrollSpeedScaleTarget(modulator.targetField) &&
                                    modulator.timeSource == MaterialScalarModulationTimeSource::ParticleLife)
                                    modulator.timeSource = MaterialScalarModulationTimeSource::EmitterTime;
                            }
                        }

                        if (!structureChanged)
                        {
                            if (Is_MaterialScrollSpeedScaleTarget(modulator.targetField) &&
                                modulator.timeSource == MaterialScalarModulationTimeSource::ParticleLife)
                            {
                                modulator.timeSource = MaterialScalarModulationTimeSource::EmitterTime;
                                changed = true;
                            }

                            detailContext.Draw_PropertyLabel("연산", "현재 scalar modulation은 대상 값에 분포 값을 곱하는 Multiply 연산입니다.");
                            ImGui::TextUnformatted(Get_MaterialScalarOperationLabel(modulator.operation));
                            if (detailContext.Draw_ResetButton(modulator.operation != defaultData.operation))
                            {
                                modulator.operation = defaultData.operation;
                                changed = true;
                            }
                            changed |= Draw_MaterialScalarTimeSourceProperty(
                                detailContext,
                                "시간 축",
                                modulator.timeSource,
                                defaultData.timeSource,
                                allowScalarParticleLife && !Is_MaterialScrollSpeedScaleTarget(modulator.targetField)
                            );
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_FloatDistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution,
                            0.01f,
                            true,
                            true,
                            nullptr,
                            "선택한 시간 축을 입력으로 대상 material scalar에 곱할 값을 평가합니다."
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
            }

            ImGui::PopID();
            ImGui::PopID();

            if (structureChanged)
                break;
        }
    }

    return changed;
}

NS_END
