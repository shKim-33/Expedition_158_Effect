#include "EffectAuthoringModuleMetadata.h"

NS_BEGIN(EffectEditor)

const AuthoringModuleMetadata& Get_AuthoringModuleMetadata(AuthoringModuleType type)
{
    static const AuthoringModuleMetadata required{
        "Required",
        L"필수",
        L"텍스처, 머티리얼",
        false,
        false,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata spawn{
        "Spawn",
        L"스폰",
        L"생성량 / 루프 기준",
        false,
        false
    };
    static const AuthoringModuleMetadata lifetime{
        "Lifetime",
        L"수명",
        L"파티클 수명 범위",
        true,
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata initialLocation{
        "Initial Location",
        L"초기 위치",
        L"생성 시점 위치 분포"
    };
    static const AuthoringModuleMetadata sphereLocation{
        "Sphere Location",
        L"구 위치",
        L"구 표면/부피 생성 위치"
    };
    static const AuthoringModuleMetadata planeRadialLocation{
        "PlaneRadialLocation",
        L"평면/방사 위치",
        L"평면 기반 사각/원반/링/호 위치"
    };
    static const AuthoringModuleMetadata cylinderLocation{
        "CylinderLocation",
        L"실린더 위치",
        L"원통 표면/부피 생성 위치"
    };
    static const AuthoringModuleMetadata initialSize{
        "Initial Size",
        L"초기 크기",
        L"생성 시점 크기"
    };
    static const AuthoringModuleMetadata initialMeshSize{
        "Initial Mesh Size",
        L"초기 메시 크기",
        L"생성 시점 메시 XYZ 크기"
    };
    static const AuthoringModuleMetadata initialVelocity{
        "Initial Velocity",
        L"초기 속도",
        L"생성 시점 벡터 속도"
    };
    static const AuthoringModuleMetadata initialRadialVelocity{
        "Initial Radial Velocity",
        L"초기 방사성 속도",
        L"생성 위치 기준 방사성 속도"
    };
    static const AuthoringModuleMetadata velocityCone{
        "VelocityCone",
        L"속도 원뿔",
        L"축 기준 원뿔 초기 속도"
    };
    static const AuthoringModuleMetadata sourceMotionVelocity{
        "SourceMotionVelocity",
        L"소스 모션 속도",
        L"source/trail motion 기반 초기 속도",
        true,
        true,
        false,
        false,
        false
    };
    static const AuthoringModuleMetadata acceleration{
        "Acceleration",
        L"가속",
        L"시간에 따라 더해지는 속도"
    };
    static const AuthoringModuleMetadata drag{
        "Drag",
        L"드래그",
        L"속도 감쇠"
    };
    static const AuthoringModuleMetadata velocityOverLife{
        "Velocity Over Life",
        L"수명에 따른 속도",
        L"수명 진행률 기준 속도 배율"
    };
    static const AuthoringModuleMetadata orbitOverLife{
        "OrbitOverLife",
        L"수명에 따른 공전",
        L"emitter pivot 기준 위치 회전"
    };
    static const AuthoringModuleMetadata initialRotation{
        "Initial Rotation",
        L"초기 회전",
        L"생성 시점 회전각"
    };
    static const AuthoringModuleMetadata planeRadialOrientation{
        "PlaneRadialOrientation",
        L"평면/방사 회전",
        L"평면/방사 위치 frame 기반 회전 의도"
    };
    static const AuthoringModuleMetadata cylinderOrientation{
        "CylinderOrientation",
        L"실린더 회전",
        L"원통 위치 frame 기반 회전 의도"
    };
    static const AuthoringModuleMetadata sphereRadialOrientation{
        "SphereRadialOrientation",
        L"구/방사 회전",
        L"구 위치 sample frame 기반 메시 회전 의도"
    };
    static const AuthoringModuleMetadata rotationOverLife{
        "Rotation Over Life",
        L"수명에 따른 회전",
        L"수명 진행률 기준 추가 회전"
    };
    static const AuthoringModuleMetadata spriteTilt{
        "SpriteTilt",
        L"스프라이트 틸트",
        L"카드 면 자체를 local X/Y 방향으로 기울임"
    };
    static const AuthoringModuleMetadata spriteTiltOverLife{
        "SpriteTiltOverLife",
        L"수명에 따른 스프라이트 틸트",
        L"수명 진행률 기준 카드 면 기울기 추가"
    };
    static const AuthoringModuleMetadata initialRotationRate{
        "Initial Rotation Rate",
        L"초기 회전 속도",
        L"초당 회전 속도"
    };
    static const AuthoringModuleMetadata rotationRateScaleByLife{
        "Rotation Rate Scale By Life",
        L"수명에 따른 회전 속도",
        L"수명 진행률 기준 회전 속도 배율"
    };
    static const AuthoringModuleMetadata initialMeshRotation{
        "Initial Mesh Rotation",
        L"초기 메시 회전",
        L"생성 시점 메시 XYZ 회전"
    };
    static const AuthoringModuleMetadata meshRotationOverLife{
        "Mesh Rotation Over Life",
        L"수명에 따른 메시 회전",
        L"수명 진행률 기준 메시 XYZ 추가 회전"
    };
    static const AuthoringModuleMetadata meshDirectionAlignOverLife{
        "MeshDirectionAlignOverLife",
        L"수명에 따른 메시 방향 정렬",
        L"수명 진행률 기준 목표 방향 정렬"
    };
    static const AuthoringModuleMetadata initialMeshRotationRate{
        "Initial Mesh Rotation Rate",
        L"초기 메시 회전 속도",
        L"초당 메시 XYZ 회전 속도"
    };
    static const AuthoringModuleMetadata meshRotationRateScaleByLife{
        "Mesh Rotation Rate Scale By Life",
        L"수명에 따른 메시 회전 속도",
        L"수명 진행률 기준 메시 XYZ 회전 속도 배율"
    };
    static const AuthoringModuleMetadata initialColor{
        "Initial Color",
        L"초기 컬러",
        L"생성 시점 색상 / 알파",
        true,
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata colorOverLife{
        "Color Over Life",
        L"수명에 따른 색상",
        L"수명 진행률 기준 색 변화",
        true,
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata subUVFrameOverLife{
        "SubUV Frame Over Life",
        L"서브UV 프레임 선택/재생",
        L"수명 진행률 기준 frame 선택",
        true,
        true,
        true,
        false,
        true
    };
    static const AuthoringModuleMetadata sizeByLife{
        "Size By Life",
        L"수명에 따른 크기",
        L"수명 진행률 기준 크기 배율",
        true,
        true,
        true,
        true,
        true
    };
    static const AuthoringModuleMetadata beamEnvelopeOverLife{
        "BeamEnvelopeOverLife",
        L"수명에 따른 빔 범위/폭",
        L"수명 진행률 기준 Beam 표시 범위/폭",
        true,
        true,
        false,
        false,
        true
    };
    static const AuthoringModuleMetadata meshSizeByLife{
        "Mesh Size By Life",
        L"수명에 따른 메시 크기",
        L"수명 진행률 기준 메시 XYZ 크기 배율",
        true,
        true,
        true,
        false
    };
    static const AuthoringModuleMetadata spawnPerUnit{
        "Spawn Per Unit",
        L"거리당 스폰",
        L"Trail sample 삽입 밀도",
        true,
        true,
        false,
        true
    };
    static const AuthoringModuleMetadata sourceHistorySpriteTrailPathFollow{
        "SourceHistorySpriteTrailPathFollow",
        L"경로 따라가기",
        L"스프라이트 트레일 stamp가 source path를 따라 이동",
        false,
        true
    };
    static const AuthoringModuleMetadata sourceHistorySpriteTrailPathReplay{
        "SourceHistorySpriteTrailPathReplay",
        L"경로 리플레이",
        L"스프라이트 트레일 stamp가 기록된 source path를 따라 head로 replay",
        false,
        true
    };
    static const AuthoringModuleMetadata ribbonOrientation{
        "RibbonOrientation",
        L"리본 방향",
        L"리본 폭 방향 기준축 / 펼침 각도",
        true,
        true,
        false,
        false,
        false
    };
    static const AuthoringModuleMetadata materialScalarModulation{
        "MaterialScalarModulation",
        L"머티리얼 변조",
        L"머티리얼 scalar 시간 변화",
        true,
        true,
        true,
        true,
        true
    };

    switch (type)
    {
    case AuthoringModuleType::Required:
        return required;
    case AuthoringModuleType::Spawn:
        return spawn;
    case AuthoringModuleType::Lifetime:
        return lifetime;
    case AuthoringModuleType::InitialLocation:
        return initialLocation;
    case AuthoringModuleType::SphereLocation:
        return sphereLocation;
    case AuthoringModuleType::PlaneRadialLocation:
        return planeRadialLocation;
    case AuthoringModuleType::CylinderLocation:
        return cylinderLocation;
    case AuthoringModuleType::InitialSize:
        return initialSize;
    case AuthoringModuleType::InitialMeshSize:
        return initialMeshSize;
    case AuthoringModuleType::InitialVelocity:
        return initialVelocity;
    case AuthoringModuleType::InitialRadialVelocity:
        return initialRadialVelocity;
    case AuthoringModuleType::VelocityCone:
        return velocityCone;
    case AuthoringModuleType::SourceMotionVelocity:
        return sourceMotionVelocity;
    case AuthoringModuleType::Acceleration:
        return acceleration;
    case AuthoringModuleType::Drag:
        return drag;
    case AuthoringModuleType::VelocityOverLife:
        return velocityOverLife;
    case AuthoringModuleType::OrbitOverLife:
        return orbitOverLife;
    case AuthoringModuleType::InitialRotation:
        return initialRotation;
    case AuthoringModuleType::SphereRadialOrientation:
        return sphereRadialOrientation;
    case AuthoringModuleType::PlaneRadialOrientation:
        return planeRadialOrientation;
    case AuthoringModuleType::CylinderOrientation:
        return cylinderOrientation;
    case AuthoringModuleType::RotationOverLife:
        return rotationOverLife;
    case AuthoringModuleType::SpriteTilt:
        return spriteTilt;
    case AuthoringModuleType::SpriteTiltOverLife:
        return spriteTiltOverLife;
    case AuthoringModuleType::InitialRotationRate:
        return initialRotationRate;
    case AuthoringModuleType::RotationRateScaleByLife:
        return rotationRateScaleByLife;
    case AuthoringModuleType::InitialMeshRotation:
        return initialMeshRotation;
    case AuthoringModuleType::MeshRotationOverLife:
        return meshRotationOverLife;
    case AuthoringModuleType::MeshDirectionAlignOverLife:
        return meshDirectionAlignOverLife;
    case AuthoringModuleType::InitialMeshRotationRate:
        return initialMeshRotationRate;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        return meshRotationRateScaleByLife;
    case AuthoringModuleType::InitialColor:
        return initialColor;
    case AuthoringModuleType::ColorOverLife:
        return colorOverLife;
    case AuthoringModuleType::SubUVFrameOverLife:
        return subUVFrameOverLife;
    case AuthoringModuleType::SizeByLife:
        return sizeByLife;
    case AuthoringModuleType::BeamEnvelopeOverLife:
        return beamEnvelopeOverLife;
    case AuthoringModuleType::MeshSizeByLife:
        return meshSizeByLife;
    case AuthoringModuleType::SpawnPerUnit:
        return spawnPerUnit;
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        return sourceHistorySpriteTrailPathFollow;
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        return sourceHistorySpriteTrailPathReplay;
    case AuthoringModuleType::RibbonOrientation:
        return ribbonOrientation;
    case AuthoringModuleType::MaterialScalarModulation:
        return materialScalarModulation;
    }

    return required;
}

void Apply_AuthoringModuleMetadata(AuthoringModule& module)
{
    const AuthoringModuleMetadata& metadata = Get_AuthoringModuleMetadata(module.type);
    module.key = metadata.key;
    module.displayName = metadata.displayName;
    module.summary = metadata.summary;
    module.movable = metadata.movable;
    module.removable = metadata.removable;
}

NS_END
