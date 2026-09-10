# VelocityOverLife Channel Ownership Problem Solving Record

문서 작성일: 2026-07-01

## Source Task Records

- `docs/tasks/03_completed/effect_velocity_over_life_channel_mask_plan.md`

이 기록은 위 완료 문서를 기준으로 재구성했다. 코드가 이후 바뀌었을 수 있으므로, 여기서 말하는 원인과 결과는 당시 작업 기준의 문제 해결 기록으로 읽는다.

## Summary

`VelocityOverLife`는 particle life progress에 따라 velocity를 스케일하는 단순한 모듈처럼 보이지만, 실제 문제는 curve 값이 아니라 velocity ownership에 있었다. 모듈식 이펙트 에디터 구조에서는 `InitialVelocity`, `InitialRadialVelocity`, `VelocityCone`, `SourceMotionVelocity`, `Acceleration` 같은 속도 모듈이 독립 UI처럼 보인다. 하지만 runtime에서는 이 계산 결과가 하나의 velocity로 합쳐져 적용 대상 제어가 어려웠다.

이 구조에서는 초반 `InitialRadialVelocity`만 줄이고 후반 `Acceleration` 발사는 유지하려는 제작 의도를 표현하기 어렵다. `VelocityOverLife`가 전체 velocity를 일괄 스케일하면, 초반 radial gather를 억제하기 위해 scale을 낮춘 순간 후반 launch motion까지 같이 눌린다.

이번 개선은 속도 모듈의 계산 결과를 channel별로 분리하고, `VelocityOverLife`가 `applyChannels`로 선택 적용되도록 만든 작업이다. 덕분에 Cascade-style 모듈 목록 구조를 유지하면서도 더 정밀한 motion ownership을 표현할 수 있게 되었다.

## Starting Problem

대표 문제는 sword mesh 계열 연출에서 드러났다. 초반에는 `InitialRadialVelocity`로 검을 모으고, 후반에는 `Acceleration`으로 빠르게 발사하려는 구성이었다.

기존 구조에서는 `VelocityOverLife`가 전체 `particle.velocity`와 최종 이동량에 적용됐다. 그래서 `InitialRadialVelocity` 감쇠를 위해 life 초반 scale을 0에 가깝게 만들면, 후반에 누적되거나 적용되어야 할 acceleration motion도 함께 줄어들었다.

제작자 관점에서는 속도 모듈들이 별도 authoring unit처럼 보였지만, runtime 관점에서는 이미 하나의 velocity로 collapse된 뒤였다. 따라서 UI에 curve를 하나 더 추가하거나 기존 scale 값을 조정하는 것만으로는 문제를 안정적으로 풀 수 없었다.

## Root Cause

원인은 `VelocityOverLife` curve의 평가 방식이 아니라, "어떤 velocity source에 이 scale을 적용할 것인가"를 표현하는 계약이 없었던 데 있었다.

기존 motion path는 여러 velocity source를 샘플링한 뒤 하나의 velocity state로 합산했다. 이후 `VelocityOverLife`는 합산된 velocity 전체를 스케일했다.

```text
InitialVelocity
InitialRadialVelocity
VelocityCone
SourceMotionVelocity
Acceleration
  -> single runtime velocity
VelocityOverLife
  -> scales the collapsed velocity
```

이 구조에서는 `InitialRadialVelocity`와 `AccelerationIntegratedVelocity`가 authoring에서는 다른 모듈이어도, `VelocityOverLife` 입장에서는 같은 대상이다. 결국 필요한 것은 새 curve가 아니라, runtime velocity를 channel별로 구분하고 각 channel의 owner를 결정하는 계약이었다.

## Decision

`VelocityOverLife`에 channel mask를 추가하고, velocity source를 아래 channel로 분리했다.

| Channel | Meaning |
| --- | --- |
| `InitialVelocity` | 일반 초기 속도 |
| `InitialRadialVelocity` | spawn center 기준 radial 초기 속도 |
| `VelocityCone` | cone 분포 기반 초기 속도 |
| `SourceMotionVelocity` | source movement에서 유래한 속도 |
| `AccelerationIntegratedVelocity` | acceleration 적분으로 생긴 속도 |

`VelocityOverLife.applyChannels`는 이 channel 중 어느 속도에 scale을 적용할지 결정한다. 기존 asset 호환을 위해 field가 없거나 비어 있는 결과는 all-channel로 보정한다.

여러 `VelocityOverLife`가 있는 경우에는 emitter module order를 ownership 기준으로 삼았다. 같은 channel을 여러 모듈이 요구하면 위쪽 enabled module이 해당 channel을 소유한다. 겹치지 않는 channel은 여러 `VelocityOverLife`가 함께 작동할 수 있다.

이 선택은 Niagara-style graph를 도입하지 않고, Cascade-style module stack 안에서 필요한 적용 대상 선택권만 여는 방식이다. 모듈식 에디터의 형태는 유지하되, runtime motion owner는 더 명확하게 나누는 쪽을 택했다.

## Applied Implementation

Authoring data와 JSON에는 `applyChannels`가 추가됐다. 저장 시 all-channel도 명시적으로 기록하고, load 시 field 누락이나 알 수 없는 token은 호환 fallback을 거쳐 all-channel 의미를 유지하게 했다.

에디터 UI는 기존 `속도 배율` 중심 흐름을 유지하면서 고급 적용 대상 체크박스 그룹을 추가했다. `VelocityOverLife`는 다중 추가가 가능해졌고, 같은 channel을 여러 모듈이 공유하면 module order 기준 ownership warning을 보여준다.

Preview lowering과 Client runtime loader는 같은 channel ownership 규칙을 사용한다. enabled `VelocityOverLife`를 module order대로 순회하고, channel별 첫 번째 owner의 curve를 runtime desc에 채운다.

Runtime consumer는 같은 계약을 각자의 motion path에 맞게 소비한다. Mesh CPU emitter, Compute Sprite shader path, SourceHistorySpriteTrail emitter는 spawn velocity channel과 acceleration-integrated velocity channel을 분리해 저장하거나 계산하고, life progress에서 channel별 scale을 평가해 최종 velocity를 만든다.

## Compatibility

기존 asset은 `applyChannels`가 없으므로 all-channel `VelocityOverLife`로 로드된다. 알 수 없는 channel token이 있어도 무시하고, 결과가 비면 all-channel로 보정한다.

단일 all-channel `VelocityOverLife`는 기존 "전체 velocity scale" 의미와 동등하게 유지하도록 설계했다. 새 구조는 기본 동작을 바꾸기 위한 것이 아니라, 필요한 asset에서 적용 대상을 더 좁게 선택할 수 있게 하기 위한 확장이다.

shared runtime desc와 compute cbuffer 확장은 append-only 원칙을 따랐다. 기존 field는 유지하고, channel별 payload는 기존 float curve runtime desc 형태를 재사용했다.

## Result

작업은 성공적으로 완료됐다. Authoring, JSON, 에디터 표시, preview lowering, Client runtime loader, Mesh, Compute Sprite, SourceHistorySpriteTrail이 같은 channel ownership 계약을 공유하게 되었다.

이제 `VelocityOverLife`는 전체 velocity만 일괄 스케일하는 모듈이 아니라, 선택한 velocity channel에만 적용될 수 있는 모듈이다. 덕분에 모듈식 이펙트 에디터 구조 안에서도 속도 계산 결과를 더 세밀하게 조합할 수 있게 되었다.

## What We Learned

모듈식 authoring UI의 단위와 runtime owner의 단위는 다를 수 있다. UI에서 모듈이 분리되어 보여도, runtime에서 같은 state로 collapse되면 제작자는 적용 대상을 제어할 수 없다.

이 문제는 curve editor나 tangent, interpolation 문제가 아니라 ownership 문제였다. 어떤 값을 어떻게 보간할지보다 먼저, 그 값이 어떤 motion source에 적용되는지를 표현해야 했다.

Cascade-style module stack에서도 필요한 경우 channel contract를 도입하면 적용 대상 선택권을 제공할 수 있다. graph editor를 도입하지 않아도, module order와 channel ownership만으로 충분히 많은 제작 의도를 표현할 수 있다.

Preview와 runtime은 같은 lowering 계약을 공유해야 한다. authoring UI에서 선택한 channel mask가 preview와 game runtime에서 다르게 해석되면, 제작자가 보는 결과와 실제 결과가 다시 갈라진다.

## Limits / Follow-up

- 회전/방향 계열 channel mask는 이번 범위에 포함하지 않았다. 필요하면 별도 설계로 다룬다.
- Niagara-style graph나 custom attribute graph 도입은 이번 작업 범위 밖이다.

## Code References

완료 문서 기준 확인해야 할 주요 위치:

- `EffectEditor/Public/EffectAuthoring_Types.h`
- `EffectEditor/Private/EffectAuthoringJsonSerializer.cpp`
- `EffectEditor/Private/EffectEditorPreviewDefinitionBuilder.cpp`
- `Client/Private/EffectAssetRuntimeLoader_Emitters.cpp`
- `Client/Private/MeshEmitter.cpp`
- `Client/Private/ComputeSourceHistorySpriteTrailEmitter.cpp`
- `Engine/Public/Particle_Types.h`
- `Engine/Private/VIBufferCom_ComputePointParticle.cpp`
- `Client/Bin/ShaderFiles/Compute/Shader_Compute_PointParticle.hlsl`
