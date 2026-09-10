# ScreenFX Desaturate Exclusion Problem Solving Record

문서 작성일: 2026-06-24

## Source Task Records

- `docs/tasks/03_completed/screenfx_skill_cue_flash_desaturate_plan.md`

이 기록은 위 완료 문서의 Phase 4B / 4.1 / 5 내용을 기준으로 정리했다. 코드가 이후 바뀌었을 수 있으므로, 여기서 말하는 원인과 결과는 당시 작업 기준의 문제 해결 기록으로 읽는다.

## Summary

`WorldDesaturate`는 frame-level ScreenFX로 구현되었기 때문에 기본적으로 화면 source 전체를 흑백화한다. 하지만 그라디언트 스킬 계열 연출에서는 world는 흑백화하되 player / monster는 원본 color로 유지해야 했다.

최종 해결은 별도 `ScreenFxExclusionMask` render target을 만들고 제외 대상을 다시 그리는 방식이 아니라, 본 렌더 중 stencil `bit1`을 남기고 ScreenFX exclude pass에서 그 bit를 검사하는 방식이었다.

이 선택으로 추가 mesh draw 없이 목표 look을 얻었고, 기존 Background UI behind-player stencil 의미도 `bit0`으로 분리해 유지할 수 있었다.

## Starting Problem

ScreenFX는 이미 합성된 scene texture를 입력으로 받아 full-screen 또는 shape 기반 postprocess를 수행한다. 이 구조는 flash / desaturate처럼 화면 단위로 작동하는 연출에는 적합하지만, shader 입장에서는 현재 픽셀이 player인지 monster인지 world인지 알기 어렵다.

원하는 결과는 아래와 같았다.

- 일반 world: desaturate 적용
- player / monster: 원본 color 유지
- UI: 기존 렌더 순서상 ScreenFX 영향 밖에 둠

단순한 `scene color -> grayscale` lerp만으로는 이 요구를 만족할 수 없었다. ScreenFX pass 직전에 "이 픽셀은 후처리에서 제외해야 한다"는 per-pixel signal이 필요했다.

## Considered Options

### 1. Shader에서 depth나 color로 추정

가장 단순해 보이는 후보는 ScreenFX shader가 depth 또는 color 특징으로 player / monster 픽셀을 추정하는 방식이었다.

이 방식은 폐기했다. Depth만으로는 player / monster / world를 안정적으로 구분할 수 없고, color 기반 추정은 asset, lighting, look-dev 변화에 취약하다. 연출이 바뀔 때마다 classification이 흔들릴 가능성이 컸다.

### 2. 별도 ScreenFxExclusionMask RT

두 번째 후보는 `R8_UNORM` 같은 별도 mask render target을 만들고, ScreenFX 직전 제외 대상을 mask pass에 다시 렌더하는 방식이었다.

이 방식은 표현력이 가장 좋다. Mask 값으로 hard exclusion뿐 아니라 soft edge, intensity, blur, dilation 같은 확장도 가능하다.

하지만 초기 목표는 hard yes/no exclusion이었다. 또한 player / monster는 skinned mesh, cloth, hair, weapon, part object가 얽혀 있어 제외 대상 재등록과 재렌더 비용이 커질 수 있었다. 이 비용을 감수하기 전에 더 싼 경로를 먼저 검증하는 편이 맞았다.

### 3. 본 렌더 중 stencil bit 기록

세 번째 후보는 이미 그려지는 player / monster draw call에서 stencil bit를 함께 남기는 방식이었다.

이 방식은 표현력은 binary mask에 가깝지만, 이번 목표에는 충분했다. 본 렌더에서 rasterized pixel에 바로 표식을 남기기 때문에 ScreenFX 같은 screen-space pass와 잘 맞고, 별도 mask RT나 추가 mesh draw가 필요 없다.

최종 구현은 이 방식을 선택했다.

## Decision

기존 player stencil은 Background UI behind-player 회피에 이미 쓰이고 있었다. 따라서 "player stencil이 있으면 ScreenFX에서 제외한다"처럼 기존 의미를 그대로 재사용하면 UI 회피와 ScreenFX exclusion 의미가 섞일 위험이 있었다.

그래서 stencil 값을 하나의 통짜 의미로 보지 않고, 8bit stencil 공간을 bitfield처럼 나누었다.

| Bit | Value | Meaning | Reader |
| --- | ---: | --- | --- |
| `bit0` | `0x01` | Background UI behind-player 회피 | Background UI pass |
| `bit1` | `0x02` | ScreenFX exclusion, 원본 color 유지 | ScreenFX exclude pass |

Object별 기록 정책은 아래처럼 정했다.

| Object | Stencil value | Meaning |
| --- | ---: | --- |
| Player body / playable parts / player weapon | `0x03` | UI 회피 대상이면서 ScreenFX exclusion 대상 |
| Monster parts / monster weapon | `0x02` | ScreenFX exclusion 대상 |
| Normal world | `0x00` | ScreenFX 적용 대상 |

핵심은 `stencil != 0` 같은 넓은 조건을 피한 것이다. Background UI는 `bit0`만 읽고, ScreenFX exclude pass는 `bit1`만 읽는다. 덕분에 monster는 ScreenFX에서는 제외되지만 Background UI 회피 대상으로는 섞이지 않는다.

## Applied Implementation

Player 계열 렌더 경로는 기존 player stencil write helper를 통해 `0x03`을 기록한다. 이는 `bit0 | bit1`을 의미한다.

```text
Player stencil write:
  StencilReadMask  = 0xff
  StencilWriteMask = 0x03
  StencilRef       = 0x03
```

Monster 계열 렌더 경로는 ScreenFX exclusion 전용 stencil write helper를 통해 `0x02`를 기록한다. 이는 `bit1`만 켠 값이다.

```text
ScreenFX exclusion stencil write:
  StencilReadMask  = 0xff
  StencilWriteMask = 0x02
  StencilRef       = 0x02
```

여기서 `0xff`는 "255라는 의미값을 저장한다"는 뜻이 아니라, 8bit 전체를 읽을 수 있다는 read mask다. 실제로 어떤 bit가 기록되는지는 `StencilWriteMask`와 `StencilRef` 조합으로 결정된다.

Background UI pass는 `bit0`만 읽는다.

```text
Background UI behind-player read:
  StencilReadMask  = 0x01
  StencilWriteMask = 0x00
  StencilRef       = 0x01
```

ScreenFX exclude pass는 `bit1`만 읽는다.

```text
ScreenFX exclude read:
  StencilReadMask  = 0x02
  StencilWriteMask = 0x00
  StencilRef       = 0x02
```

전체 흐름은 아래와 같다.

```text
Player / monster main render
-> stencil bit 기록
-> PostProcess / ScreenFX source 준비
-> ScreenFX pass 실행
-> Exclusion Mode = Marked Objects 이면 내부 exclude pass 선택
-> bit1 픽셀은 ScreenFX 적용에서 제외되고 원본 color 유지
```

Authoring 표면에는 내부 pass 이름을 노출하지 않았다. 사용자는 `ANS_ScreenFxCue`에서 `Cue Type = WorldDesaturate` 또는 `Flash`를 고르고, 필요할 때 `Exclusion Mode = Marked Objects`를 선택한다.

`FlashExclude` / `WorldDesaturateExclude`는 user-facing cue type이 아니라, `Exclusion Mode`가 marked object 제외를 요청했을 때 내부적으로 선택되는 shader pass다.

## Result

수동 확인 기준으로 `WorldDesaturate`에서 일반 world는 흑백화되고 player / monster는 원본 color를 유지했다.

추가 mask RT를 만들지 않았고, player / monster / hair / weapon / part object를 ScreenFX exclusion 때문에 한 번 더 렌더하지 않았다. 기존 Background UI behind-player 회피도 `bit0`만 읽도록 좁혀 의미 충돌 없이 유지했다.

Authoring UI는 `Cue Type`과 `Exclusion Mode` 공통 모델로 정리되었다. 따라서 사용자는 `WorldDesaturateExclude` 같은 내부 pass 이름을 고르지 않고, "marked object를 원본색으로 유지할지"만 선택하면 된다.

## What We Learned

이번 사례에서 얻은 가장 큰 점은 stencil을 단순한 on/off mask가 아니라, 프레임 안에서 여러 시스템이 공유할 수 있는 작은 per-pixel classification 공간으로 볼 수 있다는 것이다.

중요한 것은 stencil 값을 하나의 통짜 의미로 쓰지 않는 것이다. `ReadMask`, `WriteMask`, `StencilRef`를 통해 owner별 bit 의미를 분리하면, 같은 stencil buffer 안에서도 여러 기능이 공존할 수 있다.

이번에는 `bit0`을 UI 회피 의미로 보존하고, `bit1`을 ScreenFX exclusion 의미로 추가했다. 이 분리 덕분에 기존 UI 정책을 깨지 않고 ScreenFX desaturate exclusion을 추가할 수 있었다.

또 하나의 교훈은, 초기 목표가 hard binary exclusion이라면 RT mask부터 설계하는 것이 과할 수 있다는 점이다. RT mask는 표현력이 좋지만 재렌더 목록, 비용 측정, viewport state 관리가 함께 따라온다. 이번 문제에는 stencil bit가 더 작고 직접적인 해법이었다.

## Limits

Stencil bit 방식은 cheap hard mask에 가깝다. 따라서 아래 요구가 생기면 한계가 분명해진다.

- soft edge 또는 gradient exclusion
- player는 100% 제외, monster는 50%만 제외 같은 intensity 차이
- player만 제외 / monster만 제외 / effect만 제외 같은 세분화된 group policy
- transparent hair, alpha-tested material, particle VFX의 부드러운 coverage
- 제외 영역 dilation / blur / temporal smoothing
- shader에서 mask 값을 texture로 직접 sampling해야 하는 후처리

또한 ScreenFX 실행 시점까지 같은 depth-stencil buffer의 stencil 값이 유지되어야 한다. 중간 pass가 stencil을 clear하거나 다른 의미로 덮으면 exclusion signal은 깨진다.

## Fallback Criteria

아래 조건 중 하나가 생기면 `ScreenFxExclusionMask` RT fallback을 다시 검토한다.

- stencil hard edge 품질이 look-dev 목표를 만족하지 못한다.
- exclusion intensity나 soft mask가 필요하다.
- 대상군이 늘어나 stencil bit 의미만으로 관리하기 어려워진다.
- 특정 VFX 또는 transparent part를 별도 방식으로 제외해야 한다.
- 후처리 shader가 mask texture를 sample해 추가 계산을 해야 한다.

Fallback 후보 정책은 아래와 같다.

```text
RT::ScreenFxExclusionMask
format = DXGI_FORMAT_R8_UNORM
clear value = 0

0 = ScreenFX 적용
1 = 원본 color 유지
```

이 경우 최종 적용량은 아래처럼 계산할 수 있다.

```text
finalAmount = shapeMask * (1 - exclusionMask)
```

다만 RT fallback은 player / monster skinned mesh, cloth, hair, weapon, part object를 mask pass에 다시 등록하고 그려야 할 수 있다. 구현 전에는 `maskMs`, 추가 draw call 수, mask texture sample 비용, gameplay / editor preview viewport 사이의 RT state 누수 여부를 측정해야 한다.

## Reuse Guideline

비슷한 문제에서 아래 조건이면 stencil bit 방식을 먼저 검토한다.

- 제외 대상이 이미 본 렌더에서 그려진다.
- 후처리 적용 여부가 yes/no로 충분하다.
- 추가 render target과 재렌더 비용을 피하고 싶다.
- 기존 stencil 의미와 새 의미를 read mask / write mask로 분리할 수 있다.

반대로 표현력이 더 중요하거나 대상군이 많다면, 처음부터 RT mask 기반으로 설계하는 편이 낫다.

## Code References

현재 구현에서 확인해야 할 주요 위치:

- `Engine/Private/GameInstance.cpp`
  - `GameInstance::Bind_PlayerStencilWriteState()`
  - `GameInstance::Bind_ScreenFxExclusionStencilWriteState()`
  - `GameInstance::Bind_BackgroundUIBehindPlayerStencilState()`
- `Engine/Bin/ShaderFiles/Engine_Shader_RenderState.hlsl`
  - `DSS_ScreenFxExclude`
- `Client/Bin/ShaderFiles/Engine_Shader_RenderState.hlsl`
  - `DSS_ScreenFxExclude` copied shader include
- `Client/Bin/ShaderFiles/Shader_ScreenFx.hlsl`
  - `FlashExclude`
  - `WorldDesaturateExclude`
- `Client/Private/ANS_ScreenFxCue.cpp`
  - `Exclusion Mode` lowering
- `Client/Private/Flash_ScreenFx.cpp`
  - `ScreenFxExclusionMode::MarkedObjects` pass selection
- `Client/Private/WorldDesaturate_ScreenFx.cpp`
  - `ScreenFxExclusionMode::MarkedObjects` pass selection
- `Client/Private/Body_Player.cpp`
- `Client/Private/Hair_Player.cpp`
- `Client/Private/FaceAccessory_Player.cpp`
- `Client/Private/PlayableParts.cpp`
- `Client/Private/Weapon.cpp`
- `Client/Private/Monster_Part.cpp`

## Related Task Record

- `docs/tasks/03_completed/screenfx_skill_cue_flash_desaturate_plan.md`
