# Effect Trail / Ribbon Curve Correction Problem Solving Record

문서 작성일: 2026-06-14

## Source Task Records

- `docs/tasks/03_completed/effecteditor_source_history_curve_resampling_rework_plan.md`

이 기록은 위 완료 문서를 기준으로 재구성했다. 코드가 이후 바뀌었을 수 있으므로, 여기서 말하는 원인과 결과는 당시 작업 기준의 문제 해결 기록으로 읽는다.

## Summary

`SourceHistorySpriteTrail`과 `SourceHistoryRibbon`에서 sword swing arc가 폴리곤처럼 보이거나, `smoothTangent`를 켰을 때 오히려 큰 arc가 안쪽으로 눌리는 문제가 있었다.

핵심 원인은 SourceHistory 계열에 일반 Trail의 smoothing 감각을 그대로 가져온 것이었다. 일반 Trail은 base/tip geometry를 가진 strip이라 position averaging이 작은 떨림 완화에 맞을 수 있다. 하지만 SourceHistory 계열은 single-point source path가 정본이므로, position averaging은 곧 path 자체를 깎는 효과가 된다.

최종 판단은 raw source history와 render resampling의 책임을 분리하는 것이었다. SpriteTrail은 stamp center를 raw path 위에 남기고 tangent만 안정화했으며, Ribbon은 history pre-split을 제거하고 render sample 생성 단계에서 Catmull-Rom resampling을 담당하게 했다.

## Starting Problem

문제는 두 계열에서 다르게 드러났다.

### SourceHistorySpriteTrail

`smoothTangent`라는 이름과 달리 tangent만 보정하지 않고 history sample position 자체를 0.25 / 0.5 / 0.25 평균으로 이동했다. 그 결과:

- `distanceFromHead`는 raw history 기준이었다.
- stamp center와 tangent는 smoothed position 기준이었다.
- tight swing에서 stamp center가 원본 arc 안쪽으로 당겨졌다.
- Path Follow / Path Replay가 기대하는 path-distance 의미와 실제 sample 위치가 갈라질 수 있었다.

### SourceHistoryRibbon

`Insert_HistorySample()`이 긴 frame 이동을 `sampleSpacing` 기준 `Vec3::Lerp`로 미리 쪼갰다. 이 pre-split은 끊김을 줄이는 안전장치처럼 보였지만, 큰 swing arc를 작은 직선 segment로 먼저 만들어 버렸다.

그 뒤 `Build_RenderSamples()`에서 Catmull-Rom을 적용해도, 이미 쪼개진 직선 점들이 control point가 되므로 큰 arc를 되살리기 어려웠다.

## Considered Options / Compared Correction Models

| 방식 | 바꾸는 데이터 | 맞는 경우 | 문제가 된 경우 |
| --- | --- | --- | --- |
| position averaging | history/control point position | 일반 Trail base/tip 떨림 완화 | SourceHistory single-point path를 안쪽으로 누름 |
| linear pre-split | history sample position | 긴 frame 이동을 최소 선분으로 보강 | 큰 arc를 작은 직선 점으로 먼저 쪼갬 |
| Catmull-Rom render resampling | render sample position | raw path가 살아 있을 때 arc 표현 개선 | control point가 이미 눌려 있으면 효과 제한 |
| tangent-only smoothing | tangent direction | center path 보존 + 카드 방향 안정화 | sample 밀도 부족 자체는 해결하지 못함 |

## Decision

SourceHistory 계열에서는 raw source path를 먼저 보존한다.

- path를 보존해야 하는 문제와 tangent 방향 안정화 문제를 분리한다.
- history sample을 미리 직선화해서 render 단계의 선택지를 줄이지 않는다.
- render 밀도와 곡률 표현은 `Build_RenderSamples()` 쪽에서 담당한다.
- `smoothTangent`라는 이름 아래 position averaging을 숨기지 않는다.

이 결정 때문에 SourceHistory 계열은 일반 Trail의 base/tip geometry smoothing을 그대로 따라가지 않는다.

## Applied Implementation

완료 문서 기준 적용 내용은 아래와 같다.

### SourceHistorySpriteTrail

- `smoothTangent`가 stamp center position을 움직이지 않도록 raw history position 기준 sampling으로 정리했다.
- tangent만 인접 segment 방향 평균으로 안정화했다.
- Path Follow / Path Replay의 `pathDistanceFromHead` ownership은 유지했다.

### SourceHistoryRibbon

- history 삽입 단계의 `sampleSpacing` 기준 직선 pre-split을 제거했다.
- raw source history와 render resampling 책임을 분리했다.
- `Build_RenderSamples()`에서 raw source sample 사이를 Catmull-Rom으로 resample하고, render sample 기준으로 `distanceFromHead`를 재계산했다.

### Follow-up Adjustment

- `SourceHistoryRibbon` 프리셋의 `smoothTangent` 기본값을 OFF로 두고, Custom 품질에서만 사용자가 명시적으로 켤 수 있게 했다.

## Result

완료 문서 기준 결과:

- 캡처 확인 기준 Sprite Trail과 Ribbon arc 품질이 전반적으로 개선됐다.
- Ribbon은 `smoothTangent OFF`가 더 자연스러운 baseline으로 확인됐다.
- `Client/Default/Client.vcxproj` Debug x64 빌드는 통과했다.
- `EffectEditor/Default/EffectEditor.vcxproj` Debug x64는 코드 컴파일까지 통과했지만, 실행 중인 `EffectEditor.exe`가 잠겨 링크 단계에서 `LNK1168`로 중단됐다.

`smoothTangent OFF`가 더 둥글게 보인 것은 이상 현상이 아니었다. Phase 2 이후 raw source sample이 control point로 살아 있으므로 OFF는 큰 swing 정보를 보존한다. 반대로 ON이 position averaging으로 작동하면 raw sample의 급격한 방향 변화와 넓은 공간 이동을 평균으로 눌러 arc를 얌전하게 만들 수 있다.

## What We Learned

- Catmull-Rom은 control point에 남아 있는 정보를 부드럽게 연결할 뿐, 이미 지워진 큰 arc를 복원하지 않는다.
- SourceHistory 계열에서 position smoothing은 path를 부드럽게 할 수도 있지만, path를 지울 수도 있다.
- subdivision은 곡선의 모양 자체보다 화면에 드러나는 render sample 밀도를 늘리는 책임에 가깝다.
- SourceHistorySpriteTrail은 center 보존 + tangent-only가 맞았다.
- SourceHistoryRibbon은 raw history 보존 + render resampling이 1차로 맞았다.

## Limits / Open

- 일반 Trail의 base/tip geometry smoothing은 이 기록의 직접 수정 대상이 아니었다.
- Path Follow / Path Replay 정책 재설계, material/shader family 변경, preview object/gate UI 변경은 완료 문서의 비목표였다.
- Ribbon `smoothTangent`의 장기 의미는 tangent-only로 바꿀지, 별도 이름으로 분리할지 후속 판단이 필요할 수 있다.

## Code References

완료 문서 기준 확인해야 할 주요 위치:

- `Client/Private/ComputeSourceHistorySpriteTrailEmitter.cpp`
- `Client/Public/ComputeSourceHistorySpriteTrailEmitter.h`
- `Client/Private/ComputeSourceHistoryRibbonEmitter.cpp`
- `Client/Public/ComputeSourceHistoryRibbonEmitter.h`
- `Client/Bin/ShaderFiles/Shader_EffectRibbon.hlsl`
- `Client/Bin/ShaderFiles/Shader_EffectRibbonDistortion.hlsl`
