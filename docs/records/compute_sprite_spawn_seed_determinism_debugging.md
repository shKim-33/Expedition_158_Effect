# Compute Sprite Spawn Seed Determinism Problem Solving Record

문서 작성일: 2026-06-19

## Source Task Records

- `docs/tasks/03_completed/effect_compute_sprite_spawn_seed_determinism_plan.md`

이 기록은 위 완료 문서를 기준으로 재구성했다. 코드가 이후 바뀌었을 수 있으므로, 여기서 말하는 원인과 결과는 당시 작업 기준의 문제 해결 기록으로 읽는다.

## Summary

`ComputeSpriteEmitter`에서 `useInstanceSeed=false`인데도 Restart Preview마다 random 결과가 바뀌는 문제가 있었다. 재현 조건은 `RandomTest2.effect.json`에서 `burst count=16`, `maxParticleCount=64`, `InitialVelocity=Uniform`, `Lifetime=Constant`, `processSpawnRate=false` 조합이었다.

원인은 seed salt가 흔들린 것이 아니라 GPU compute shader의 unordered slot allocation이었다. random identity가 particle slot identity에 묶여 있었기 때문에, 같은 spawn request라도 어떤 inactive slot이 먼저 spawn을 가져가느냐에 따라 `respawnSeed` 집합이 달라졌다.

최종 해결은 slot index가 아니라 replay-local spawn serial을 random seed base로 쓰는 방식이었다. `ComputeSpriteEmitter`가 replay-local serial counter를 소유하고, shader는 `spawnSerial = spawnSerialBase + spawnCandidate`로 새 spawn의 `respawnSeed`를 만든다.

## Starting Problem

재현은 아래 조건에서 뚜렷했다.

- `maxParticleCount=64`, `burst count=16`: Restart Preview 반복 시 초기 속도 패턴이 일정 주기로 바뀜
- `maxParticleCount=16`, `burst count=16`: 같은 asset에서 현상이 사라짐
- `useInstanceSeed=false`: 재생마다 변주를 의도적으로 켠 상태가 아님

이 차이는 field별 seed salt나 hash 식보다, inactive slot 집합과 spawn assignment가 문제라는 쪽을 가리켰다. 모든 slot이 곧바로 active가 되는 조건에서는 slot 선택 변수가 사라지고, 일부 slot만 active가 되는 조건에서는 어떤 slot이 선택됐는지가 random 결과에 섞였다.

## Investigation

완료 문서 기준 당시 compute path는 아래처럼 해석했다.

```text
inactive slot
-> InterlockedAdd(..., spawnCandidate)
-> spawnCandidate < g_SpawnRequest 이면 active
-> lifecycle.placementIndex = spawnCandidate
-> lifecycle.respawnSeed 기반으로 field random sample
```

중요한 구분은 세 가지였다.

| 개념 | 의미 | random identity로 적합한가 |
| --- | --- | --- |
| slot index | particle state 저장 위치 | 부적합. GPU scheduling에 따라 어떤 slot이 spawn될지 달라질 수 있음 |
| spawnCandidate | 이번 dispatch 안의 spawn 순번 | 일부 적합. dispatch-local이라 spawn rate에서 반복될 수 있음 |
| replay-local spawn serial | 한 replay 안에서 누적되는 spawn ordinal | 적합. Restart Preview에서는 반복되고 한 재생 안에서는 중복되지 않음 |

`InterlockedAdd`는 counter 값을 유일하게 나눠줄 뿐, 어떤 thread가 먼저 값을 받을지는 보장하지 않는다. 따라서 slot index를 seed owner로 쓰면 seed salt와 hash 식이 고정되어도 결과가 흔들릴 수 있다.

## Considered Options

### 1. Slot index를 계속 사용

기존 방식에 가장 가까웠지만 문제의 원인을 그대로 남긴다. `spawnRequest < maxParticleCount`에서 어떤 inactive slot이 spawn되는지가 random seed 집합을 바꾸므로 Restart Preview 안정성을 보장하지 못한다.

### 2. `spawnCandidate`만 사용

slot identity와 random identity를 분리하는 데는 도움이 된다. 하지만 `spawnCandidate`는 dispatch마다 0부터 시작하므로 spawn rate처럼 여러 dispatch에 걸쳐 입자가 태어나는 경우 같은 random 패턴이 반복될 수 있다.

### 3. Replay-local spawn serial 사용

선택한 방식이다. `ComputeSpriteEmitter`가 replay reset 때 0으로 초기화되는 serial counter를 갖고, 각 dispatch의 base serial을 shader에 전달한다. shader는 `spawnSerialBase + spawnCandidate`를 새 spawn의 random identity로 사용한다.

## Decision

random seed base는 particle slot이 아니라 spawn ordinal이 소유한다.

```text
ComputeSpriteEmitter
-> 이번 dispatch의 spawnSerialBase 계산
-> VIBufferCom_ComputePointParticle::Dispatch_Compute(..., spawnSerialBase, ...)
-> cbuffer spawnSerialParams.x 전달
-> shader에서 spawnSerial = spawnSerialParams.x + spawnCandidate
-> lifecycle.respawnSeed = spawnSerial * 9781 + 1
```

`placementIndex`와 `placementCount`는 even placement 용도로 유지한다. 이 값들은 배치 순서이고, random identity의 정본은 아니다.

`useInstanceSeed`의 의미도 바꾸지 않는다. `useInstanceSeed=true`는 재생 단위 변주를 허용하는 옵션이고, GPU slot allocation 결정성을 보장하는 옵션이 아니다.

## Applied Implementation

완료 문서 기준 적용 내용은 아래와 같다.

- `ComputeSpriteEmitter`가 replay-local spawn serial counter를 소유한다.
- Restart Preview reset에서 counter를 0으로 초기화한다.
- `Advance_Playback()`이 이번 dispatch의 `spawnSerialBase`를 계산해 compute dispatch 경로로 전달한다.
- `ComputePointParticleParams`와 `Shader_Compute_PointParticle.hlsl` cbuffer에 append-only로 `spawnSerialParams` / `g_SpawnSerialParams`를 추가했다.
- shader는 새 spawn 시에만 `lifecycle.respawnSeed`를 `spawnSerial` 기반으로 설정한다.
- 기존 active particle은 저장된 lifecycle seed를 유지한다.

## Result

완료 문서 기준 결과:

- `maxParticleCount=64`, `burst=16` 조건에서도 Restart Preview random 흔들림이 해결됐다.
- `maxParticleCount=16`, `burst=16` 조건은 기존처럼 안정적이었다.
- `useInstanceSeed=true`의 replay variation 의미는 유지했다.
- `Engine`, `Client`, `EffectEditor` Debug x64 빌드가 성공했고, `Shader_Compute_PointParticle.cso` 재컴파일을 확인했다.

## What We Learned

- random seed가 흔들린 것이 아니라 random의 주인이 흔들릴 수 있다.
- slot identity는 저장 위치이고, spawn serial은 태어난 순서다.
- `spawnCandidate`는 dispatch-local 배치 순서로는 유용하지만, 장기 random identity로는 부족할 수 있다.
- cbuffer 변경은 CPU struct, HLSL layout, shader rebuild를 한 세트로 검증해야 한다.

## Limits / Deferred

- CPU 기반 emitter family까지 같은 방식으로 확장하지 않았다. 같은 원인이 확인되지 않는 한 Sprite compute 경로로 제한한다.
- `useInstanceSeed=false` + default seed에서 lifetime uniform이 CPU 초기 lifecycle과 shader respawn lifecycle 사이에 다른 sampling policy를 갖는 문제는 완료 문서에서 별도 deferred로 남겼다.

## Code References

완료 문서 기준 확인해야 할 주요 위치:

- `Client/Private/ComputeSpriteEmitter.cpp`
- `Client/Public/ComputeSpriteEmitter.h`
- `Engine/Private/VIBufferCom_ComputePointParticle.cpp`
- `Engine/Public/VIBufferCom_ComputePointParticle.h`
- `Engine/Public/Particle_Types.h`
- `Client/Bin/ShaderFiles/Compute/Shader_Compute_PointParticle.hlsl`
