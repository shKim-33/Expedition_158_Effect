# AnimNotifyState Lifecycle Cleanup Problem Solving Record

작성일: 2026-06-16

## Source Task Records

- `docs/tasks/03_completed/anim_notify_state_lifecycle_fix_plan.md`

이 기록은 위 완료 문서를 기준으로 재구성했다. 코드가 이후 바뀌었을 수 있으므로, 여기서 말하는 원인과 결과는 당시 작업 기준의 문제 해결 기록으로 읽는다.

## Summary

FreeAim 조준 루프에서 SpriteTrail 이펙트가 애니메이션 종료 후에도 계속 생성되는 문제가 있었다. 처음에는 `SourceHistorySpriteTrail` 또는 개별 effect cleanup 문제처럼 보였지만, 완료 문서 기준 핵심 원인은 `AnimNotifyState` 종료 보장 취약점이었다.

`SourceHistorySpriteTrail`은 문제를 잘 드러낸 관측자였다. 진짜 구조적 문제는 `ANS_*`가 `On_Begin()`에서 연 리소스와 gameplay state를 모든 애니메이션 전환 경로에서 반드시 `On_End()`로 닫을 수 있는가였다.

최종 해결은 `ModelCom`의 stale `AnimNotifyState` cleanup을 현재 animation notify data entry 존재 여부와 분리하는 것이었다. notify data entry가 없는 animation으로 전환해도 이전 ANS가 `On_End()`를 받을 수 있게 정리했다.

## Starting Problem

대표 증상은 FreeAim 종료 후에도 SourceHistorySpriteTrail stamp가 계속 새로 생기는 것이었다.

완료 문서 기준 조건은 아래와 같았다.

- FreeAim 조준 루프에서 `ANS_PlayEffect` 또는 `ANS_SourceHistoryEffect`가 시작된다.
- `FreeAimShot_Hand_EX.effect.json`은 `SourceHistorySpriteTrail` emitter를 포함한다.
- 해당 emitter는 `loopCount = 0` 무한 반복 playback을 사용한다.
- animation 종료/전환 경로에서 `On_End()`가 오지 않으면 effect stop, source sample disable, drain, pool release가 시작되지 않는다.

`ANS_SourceHistoryEffect`로 바꿔도 같은 증상이 난다는 점은 개별 ANS 구현보다 lifecycle end 보장 쪽이 문제라는 신호였다.

## Investigation

문제 당시 구조는 아래처럼 해석됐다.

```text
ModelCom::Step_FinalizeAnimationFrame()
  -> Ensure_AnimNotifyAssetLoaded()
  -> Find_CurrentNotifyAnimation()
  -> Update_AnimNotifies()
  -> Update_AnimNotifyStates()
       -> Stop_NotifyStates_ExceptAnimation()
```

`Stop_NotifyStates_ExceptAnimation()`는 이전 애니메이션에서 active 상태였던 notify state를 종료하는 핵심 cleanup 경로다. 그런데 이 함수가 `Update_AnimNotifyStates()` 안에 있으므로, 다음 animation에 notify data entry가 없으면 cleanup 경로에 도달하지 못할 수 있었다.

이 취약점은 `AN_*`보다 `ANS_*`에서 위험하다.

- `AN_*`: 한 번 `Execute()`되고 active state로 남지 않는다.
- `ANS_*`: `On_Begin()`에서 리소스나 gameplay state를 열고 `On_End()`에서 되돌린다.

따라서 현재 animation에 notify data가 없어도, 이전에 begin된 ANS가 있으면 stale cleanup은 반드시 실행되어야 한다.

## Considered Options

### 1. SourceHistorySpriteTrail playback을 수정

증상을 직접 드러낸 emitter를 고치는 방식이다. 하지만 `ANS_PlayEffect`, `ANS_SourceHistoryEffect`, `ANS_Trail`, visibility/time scale/movement 계열 ANS가 모두 같은 lifecycle gap에 영향을 받을 수 있으므로 근본 해결이 아니었다.

### 2. FreeAim asset 또는 빈 notify JSON으로 우회

특정 전환 대상 animation에 빈 notify data entry를 추가하면 cleanup 경로를 타게 만들 수 있다. 하지만 누락 animation이 생길 때마다 문제가 재발하고, ANS lifecycle의 진짜 보장 문제는 남는다.

### 3. `ModelCom` stale cleanup 경로 보강

선택한 방식이다. current animation notify data lookup 성공 여부와 stale active state cleanup을 분리한다.

## Decision

`ModelCom`이 active notify state cleanup을 current animation notify data 존재 여부에 종속시키지 않는다.

계약은 아래처럼 잡았다.

- 현재 animation name은 notify data entry가 없어도 구할 수 있다.
- active notify state의 `animationName`이 현재 animation name과 다르면 stale state로 본다.
- stale state는 current animation notify data가 없어도 `On_End()`를 받아야 한다.
- current animation의 notify begin/tick/end 판정은 해당 animation notify data가 있을 때만 수행한다.
- non-loop finished cleanup은 기존 `Stop_AllNotifyStates()` 의미를 유지한다.

## Applied Implementation

완료 문서 기준 적용 내용:

- `Stop_NotifyStates_ExceptAnimation(...)` 호출을 `Update_AnimNotifyStates()` 밖으로 이동했다.
- `Step_FinalizeAnimationFrame()`에서 current animation notify data lookup 전에 stale cleanup이 실행되게 했다.
- 따라서 current animation에 notify data entry가 없어도, animation name만 있으면 stale `AnimNotifyState` cleanup은 `Find_CurrentNotifyAnimation()` 성공 여부에 묶이지 않는다.
- `ANS_PlayEffect::On_End()`와 `ANS_SourceHistoryEffect::On_End()`는 각각 `EffectCom::Stop_Effect(...)`, `EffectCom::Stop_SourceHistoryEffect(...)` 정리 경로를 유지했다.

## Result

완료 문서 기준 결과:

- FreeAim 종료 후 `SourceHistorySpriteTrail`의 신규 stamp 생성 누수는 재현 경로에서 닫혔다.
- notify data entry가 없는 animation으로 전환해도 이전 ANS가 `On_End()`를 받는다.
- `Client`와 `MainEditor` Debug x64 빌드가 성공했다.
- Lune / Maelle / Gustave FreeAim 종료 smoke와 주요 stateful ANS regression 확인까지 완료했다.

## What We Learned

- 무한 반복 SourceHistorySpriteTrail은 End 누락을 매우 잘 드러낸다. 하지만 관측자가 곧 원인은 아니다.
- stateful ANS의 cleanup contract는 현재 animation notify data 존재 여부와 분리되어야 한다.
- 빈 notify JSON은 빠른 우회가 될 수 있지만, lifecycle gap을 해결하지 않는다.
- `On_End()`에서 상태를 복구하는 ANS가 늘어날수록 cleanup owner는 개별 ANS가 아니라 animation runtime 쪽에서 보장해야 한다.

## Limits / Open

- `SourceHistorySpriteTrail` emitter playback 구조 재설계는 이 작업의 범위가 아니었다.
- EffectEditor authoring UI 개편, AN/ANS API 전면 재설계도 범위 밖이었다.
- `_skipInitialBlendSourceNotify`, preview / notify execution disabled 같은 경계는 완료 문서에서 기존 의도를 유지하는 방향으로 다뤘다.

## Code References

완료 문서 기준 확인해야 할 주요 위치:

- `Engine/Private/ModelCom.cpp`
- `Engine/Public/ModelCom.h`
- `Client/Private/ANS_PlayEffect.cpp`
- `Client/Private/ANS_SourceHistoryEffect.cpp`
- `Client/Private/EffectCom.cpp`
- `Client/Private/ComputeSourceHistorySpriteTrailEmitter.cpp`
