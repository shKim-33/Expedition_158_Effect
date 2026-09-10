# ChangeWeapon Preview Trail UI Stencil Occlusion Problem Solving Record

문서 작성일: 2026-07-07

## Source Task Records

- `docs/tasks/01_planned/changeweapon_preview_trail_ui_occlusion_plan.md`

이 기록은 ChangeWeapon 무기 프리뷰 trail 가시성 문제를 해결하는 과정에서 드러난 오진, 실패한 방향, 최종 원인 판단을 남기기 위한 문서다. 코드가 이후 바뀔 수 있으므로, 여기서 말하는 원인과 결과는 당시 작업 기준의 문제 해결 기록으로 읽는다.

## Summary

ChangeWeapon 화면에서 Gustave / Maelle의 일부 무기 trail이 플레이어 실루엣 위에서만 보이고, 실루엣 밖에서는 검은 Background UI에 흡수되는 것처럼 사라지는 문제가 있었다.

초기에는 trail material tint, Render_Blend DSV, UI 배경 이미지, 특정 weapon effect asset 문제를 의심했다. 하지만 최종 원인은 trail 색이나 material이 아니라, ChangeWeapon preview에서 Background UI가 `player stencil` 영역만 피해 그리는 렌더 계약에 weapon trail이 참여하지 않았던 것이다.

즉 이 문제는 순수 trail material 문제가 아니라, UI Background pass와 preview attached effect 사이의 stencil exclusion 계약 누락이었다. Trail emitter는 이 계약 누락을 가장 잘 드러낸 소비자였고, 별도로 trail sample 공급 종료 후 마지막 1세그먼트가 남는 history cleanup 문제도 함께 발견되었다.

## Starting Problem

대표 증상은 아래와 같았다.

- ChangeWeapon 화면에서 무기 trail이 플레이어 몸 위에서는 보인다.
- 같은 trail이 플레이어 실루엣 밖으로 나가면 거의 보이지 않거나 완전히 사라진다.
- tint를 올리면 플레이어 위에 겹친 trail만 과하게 밝아지고, 검은 배경 밖의 trail은 여전히 살아나지 않는다.
- trail 생성이 끝난 뒤 마지막 trail 일부가 실처럼 화면에 오래 남는 경우가 있다.

처음에는 `Gustave + Lanceram` 조합에서 특히 잘 보였지만, 이후 Gustave / Maelle의 무기 3종씩 총 6개 무기 프리뷰에서 같은 계열의 문제로 보는 것이 맞았다.

## Investigation

ChangeWeapon 화면의 렌더 구조는 아래처럼 해석됐다.

```text
3D preview player / weapon / attached trail render
-> Background UI render
-> Foreground UI render
```

Background UI는 플레이어와 무기 위를 덮지 않기 위해 `Bind_BackgroundUIBehindPlayerStencilState()`를 사용한다. 이 state는 stencil `bit0`이 찍힌 영역을 피해서 Background UI를 그린다.

당시 player 계열 렌더 경로는 아래처럼 stencil을 기록하고 있었다.

- `Body_Player`
- `PlayableParts`
- `Hair_Player`
- `FaceAccessory_Player`
- `Weapon`

이들은 `Bind_PlayerStencilWriteState(...)`를 통해 player stencil을 남긴다. 따라서 Background UI는 player / weapon 실루엣 위를 피한다.

하지만 attached weapon trail은 `RenderGroup::Blend`에서 effect emitter로 그려지고, player stencil을 기록하지 않았다. 그 결과 trail이 먼저 HDR에 그려져도, 이후 Background UI pass가 player stencil 밖 영역을 다시 덮으면서 trail이 사라졌다.

그래서 화면상으로는 "trail이 플레이어 위에서만 렌더된다"처럼 보였다. 실제로는 trail draw 자체가 없는 것이 아니라, 후속 Background UI draw에 의해 player stencil 밖의 trail 픽셀이 overwrite되는 구조였다.

## Failed Directions

### 1. Render_Blend DSV 진단

Release 재현 환경에서 `Render_Blend`의 DSV 바인딩을 확인하는 로그 진단을 먼저 진행했다. 이는 `_DEBUG` 전용 state dump를 Release로 확장하지 않고 원인 후보를 줄이는 데는 의미가 있었다.

하지만 최종 원인은 Blend pass의 DSV 선택 자체가 아니었다. Blend effect가 그려진 뒤 Background UI가 덮는 구조였으므로, DSV 진단만으로는 핵심 원인에 도달할 수 없었다.

### 2. Background UI 비활성화

증상상 검은 배경이 trail을 가리는 것처럼 보였기 때문에 Background UI를 끄는 방향을 검토했다.

이 방향은 문제를 완전히 해결하는 모델이 아니었다. `Image_ChangeWeapon_BG`는 ChangeWeapon 화면의 의도된 배경 이미지이므로 끄면 UI 구조를 파괴한다. 다만 `Image_BlackBG`처럼 완전한 검은 공용 덮개 하나를 닫는 것은 ChangeWeapon 화면 구성상 유지 가능한 별도 조치로 남았다.

중요한 교훈은 "배경이 trail을 가린다"가 곧 "배경을 꺼야 한다"는 뜻은 아니라는 점이다. 이 경우에는 Background UI가 피해야 할 stencil 영역에 trail을 포함시키는 것이 맞았다.

### 3. Trail material tint override

trail visible emitter에 material tint override를 적용하는 방향도 시도했다. 이 접근은 실패했고, 오히려 플레이어 위에 이미 보이던 trail만 과하게 밝아졌다.

이 실패는 원인 판별에 중요한 신호였다. 색을 키워도 player stencil 밖에서 trail이 살아나지 않는다면, 해당 픽셀은 어둡게 그려지는 것이 아니라 후속 draw에 의해 덮이는 가능성이 크다. 즉 material tint는 occluded / overwritten pixel을 복구할 수 없다.

이 방향은 `Image_ChangeWeapon_BG`와 Background UI를 더 건드리지 않는다는 원칙을 세운 뒤 폐기했다.

## Early Detection Signals Missed

이번 문제에서 더 일찍 포착했어야 할 신호는 아래와 같다.

- "플레이어 위에서만 보인다"는 증상은 material 밝기보다 stencil / mask / 후속 UI overwrite 신호에 가깝다.
- player와 weapon은 Background UI를 뚫고 보이는데 trail만 사라진다면, "trail이 player와 같은 Background UI exclusion 계약에 참여하는가"를 먼저 확인했어야 한다.
- tint를 올려도 검은 배경 밖에서 전혀 개선되지 않는다면 material 문제가 아니라 렌더 순서 또는 stencil test 문제로 방향을 전환해야 했다.
- 특정 weapon 하나가 아니라 Gustave / Maelle 여러 무기에 반복된다면 asset 개별 문제가 아니라 shared preview render contract 문제일 가능성이 높다.
- Background UI를 끄는 실험은 원인 분리에 도움을 줄 수 있지만, UI 의도 자체를 변경하는 fix로 승격하면 안 된다.

## Decision

최종 판단은 아래와 같다.

주 가시성 문제는 trail material 문제가 아니라, ChangeWeapon preview의 Background UI stencil exclusion 계약에 weapon trail이 포함되지 않은 UI-effect 렌더 통합 문제다.

따라서 해결 방향은 trail의 색을 강제로 키우거나 UI 배경을 끄는 것이 아니라, ChangeWeapon preview 중 visible trail이 player / weapon과 같은 `bit0` stencil exclusion 대상이 되도록 하는 것이다.

다만 모든 effect에 무조건 player stencil을 쓰면 다른 Background UI 화면에 의도치 않은 구멍을 만들 수 있다. 그래서 당시 구현은 ChangeWeapon preview가 활성일 때의 `ComputeTrailEmitter` 렌더에 한정했다. 또한 mask / black / distortion 성격의 trail emitter는 Background UI 회피 대상으로 삼지 않도록 제외했다.

별도로 마지막 실선 잔상은 UI stencil 문제가 아니라 trail history 종료 처리 문제로 분리했다. sample 공급이 끊긴 뒤 visible sample이 2개 이하만 남으면 history와 draw count를 비워, 마지막 1세그먼트가 계속 남지 않게 했다.

## Applied Implementation

완료 당시 핵심 구현은 아래 경로에 있었다.

### ChangeWeapon preview trail stencil write

`Client/Private/ComputeTrailEmitter.cpp`의 `Render()`에서 ChangeWeapon preview가 활성이고, distortion / mask / black 성격이 아닌 trail에 대해 draw 직전 `Bind_PlayerStencilWriteState(false)`를 적용했다.

```text
ComputeTrailEmitter::Render()
-> ChangeWeapon preview active 확인
-> mask / black / distortion emitter 제외
-> 기존 OM depth-stencil state 저장
-> player stencil write state 바인딩
-> trail draw
-> 기존 OM depth-stencil state 복구
```

`depthWrite=false`를 사용한 이유는 trail이 Background UI 회피용 stencil만 남기면 충분하고, depth buffer를 새로 쓰는 것이 목적이 아니었기 때문이다.

### Preview active state query

`Client/Public/UIPreviewManager.h`에는 아래 읽기용 accessor만 추가했다.

```text
UIPreviewManager::Is_Active()
UIPreviewManager::Get_ActivePanel()
```

이는 `ComputeTrailEmitter`가 ChangeWeapon preview 한정 분기를 걸기 위한 최소 공개면이었다.

### Trail tail history cleanup

`Client/Private/ComputeTrailEmitter.cpp`의 `Update_History()`에서는 sample 공급이 끊긴 뒤 visible sample이 2개 이하로 줄면 history와 draw count를 즉시 비웠다.

```text
if (!sampleAccepted && visibleSampleCount <= 2)
    clear history and draw count
```

이 처리는 trail drain 중 긴 꼬리 전체를 없애는 것이 아니라, 마지막 1세그먼트 실선이 계속 남는 상태를 막기 위한 종료 조건이다.

### Removed / Reverted Direction

실패한 material tint override 경로는 제거했다.

- `ComputeTrailEmitter::Set_MaterialTintOverride()` 추가는 되돌렸다.
- `ChangeWeapon_Panel`에서 weapon별 emitter id tint override를 매 프레임 적용하던 경로도 제거했다.
- `Image_ChangeWeapon_BG`는 끄지 않았다.

현재 ChangeWeapon 화면에서 닫는 UI는 `Image_BlackBG` 하나뿐이다. 이는 의도된 `Image_ChangeWeapon_BG`와 별개의 공용 검은 덮개다.

## Result

Release x64 기준으로 `Client`와 `MainEditor` 빌드가 성공했다. 이후 Release `MainEditor.exe`를 실행해 ChangeWeapon preview에서 문제를 확인했다.

결과적으로 trail은 player 실루엣 밖에서도 Background UI에 덮이지 않고 보이게 되었다. 또한 trail 생성 종료 후 마지막 부분이 실처럼 남는 증상도 trail history cleanup으로 함께 닫혔다.

## What We Learned

가장 큰 교훈은 "보이는 곳이 player 위뿐"이라는 증상을 material 문제로 좁히면 안 된다는 것이다. 이 증상은 오히려 player-only stencil, mask, 후속 UI pass overwrite 같은 렌더 계약 문제를 먼저 의심해야 한다.

Trail emitter는 이번 문제의 원인 전체가 아니라, UI-effect 렌더 계약 누락을 드러낸 관측자이자 일부 수정 지점이었다. 정확한 분류는 `Trail Bug`보다 `UI-Effect Render Contract / Stencil Occlusion`에 가깝다.

또한 fix 후보가 UI를 끄는 방식으로 흘러갈 때는 "어떤 UI가 의도된 배경이고, 어떤 UI가 공용 덮개인가"를 먼저 분리해야 한다. 이번 경우 `Image_ChangeWeapon_BG`를 끄는 것은 UI 파괴였고, `Image_BlackBG`만 닫는 것은 별도 정책이었다.

마지막으로, 실패한 방향도 증거로 활용해야 한다. Tint override가 player 위 trail만 과하게 밝히고 player 밖 trail을 살리지 못했다는 사실은, color 문제가 아니라 후속 draw overwrite 문제라는 강한 단서였다.

## Limits / Open

현재 해결은 ChangeWeapon preview의 `ComputeTrailEmitter` 경로에 한정되어 있다. 다른 preview 화면이나 다른 effect family가 Background UI 뒤에 보여야 하는 요구가 생기면, 그 화면의 UI 렌더 순서와 stencil 계약을 별도로 검토해야 한다.

Mask / black / distortion emitter 제외는 emitter name과 material family를 함께 이용한 좁은 runtime guard다. Effect authoring naming이 크게 바뀌면 이 guard가 충분한지 다시 확인해야 한다.

모든 preview effect를 player stencil 대상으로 일반화하지 않았다. 이는 의도치 않은 Background UI hole을 막기 위한 선택이다. 향후 여러 화면에서 같은 요구가 반복되면, ad hoc 분기보다 preview render contract를 명시적으로 표현하는 API나 effect role을 검토할 수 있다.

Trail tail cleanup은 마지막 1세그먼트 잔상 제거를 목표로 한다. 만약 특정 trail asset이 intentional final linger를 요구한다면, 별도 authoring parameter나 drain policy가 필요할 수 있다.

## Reuse Guideline

비슷한 증상이 다시 나오면 아래 순서로 확인한다.

1. 해당 object / effect가 실제로 그려지는지 RenderDoc / PIX / 로그로 확인한다.
2. 그려진 뒤 후속 UI나 postprocess가 덮는지 렌더 순서를 확인한다.
3. player / weapon / effect가 같은 stencil exclusion 계약에 참여하는지 확인한다.
4. tint, alpha, material intensity는 overwrite 가능성을 배제한 뒤에 본다.
5. 특정 asset 하나가 아니라 여러 asset에서 반복되면 shared render contract를 먼저 의심한다.

Background UI가 "behind player"처럼 동작하는 화면에서는 아래 질문을 먼저 던진다.

```text
Background UI가 피하는 stencil bit는 무엇인가?
그 bit를 누가 쓰고 있는가?
이번에 보여야 하는 object/effect도 그 bit를 써야 하는가?
그 bit를 쓰면 다른 UI에 구멍을 만들 가능성은 없는가?
```

## Code References

현재 구현에서 확인해야 할 주요 위치:

- `Client/Private/ComputeTrailEmitter.cpp`
  - `ComputeTrailEmitter::Render()`
  - `ComputeTrailEmitter::Update_History()`
- `Client/Public/UIPreviewManager.h`
  - `UIPreviewManager::Is_Active()`
  - `UIPreviewManager::Get_ActivePanel()`
- `Client/Private/ChangeWeapon_Panel.cpp`
  - `Close_ChangeWeaponBlackBackground()`
  - `Open()`
  - `Open_AsProgressionContent()`
- `Engine/Private/UI_Image.cpp`
  - Background UI render group의 `Bind_BackgroundUIBehindPlayerStencilState()` 사용
- `Engine/Private/GameInstance.cpp`
  - `Bind_PlayerStencilWriteState()`
  - `Bind_BackgroundUIBehindPlayerStencilState()`
- `Client/Private/Body_Player.cpp`
- `Client/Private/PlayableParts.cpp`
- `Client/Private/Hair_Player.cpp`
- `Client/Private/FaceAccessory_Player.cpp`
- `Client/Private/Weapon.cpp`

## Related Reference

- `docs/records/screenfx_stencil_bit_exclusion_reference.md`
