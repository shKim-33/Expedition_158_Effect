# AGENTS.md

## 저장소 기본 원칙

- 기본 문맥: Visual Studio 2026, x64, C++20, DirectX11, `vcpkg` manifest mode.
- 이 저장소의 기본 셸은 PowerShell 7으로 본다. 콘솔 명령은 `cmd` 전용 문법이나 bash 관용 구문을 가정하지 않는다.
- 현재 checkout의 실제 파일, 프로젝트 설정, 출력/실행 경로를 기준으로 판단한다. 다른 DX11 저장소의 경로나 규칙을 자동 이식하지 않는다.
- `EngineSDK`는 generated/staging output이며 구현 정본이 아니다. include/lib 소비 경계나 staging 문제처럼 필요한 경우에만 확인하고, routine mirror diff는 정리 대상으로 보지 않는다.

## 판단 기준

- 호출 편의보다 실제 소유권, 런타임 경계, 정본을 우선한다.
- 구조, runtime boundary, resource table 정본이 헷갈리면 `docs/architecture/project_architecture_reference.md`를 먼저 읽고 실제 코드 경로로 검증한다.
- 런타임 동작의 정본은 가까운 로그나 미러 문서가 아니라 실제 호출 경로와 소유 객체에서 찾는다.
- 새 기능을 읽거나 수정할 때는 `authoring source -> runtime input -> mutable runtime owner`를 분리해서 판단한다.
- 현재 구현이 존재한다는 사실만으로 의도나 판단이 정당하다고 보지 않는다. 이름, 문서, 호출 계약, 런타임 의미가 어긋나면 먼저 불일치를 드러낸다.
- effect tool은 Cascade-style 편집 모델을 목표로 하며 Niagara-style node/script graph 편집은 현재 범위 밖이다. 신규 effect particle runtime/backend 판단은 compute-first를 기본으로 본다.

## 승인 경계

- 목표, 책임 위치, runtime boundary가 불명확하거나 여러 프로젝트와 빌드 구조가 얽히면 먼저 짧은 계획을 세운다.
- 승인된 목표 안에서 필요한 private C++, 헤더, shader와 인접 구현 파일은 예상 목록에 없더라도 계획을 보정하고 별도 재승인 없이 계속 진행한다.
- 계획의 오류나 누락은 중간 보고와 문서 갱신으로 처리하며, 아래 재승인 조건에 해당하지 않으면 작업을 멈추지 않는다.
- 예정에 없던 JSON, XLSX, asset data 수정 또는 목표와 외부 동작의 중대한 변경은 다시 승인받는다.
- `Engine`, shared editor/core, importer/asset pipeline, public API, 프로젝트 구조, 대규모 리팩터링, 책임 이동, 새 문법·라이브러리·공용 패턴은 사전 승인 대상으로 유지한다.

## 코드 수정 규칙

- 작은 수정도 실제 소비자, arg 흐름, key 소유자, 호출 경로를 먼저 확인한다.
- `Engine/Bin/ShaderFiles/Engine_Shader_*` 또는 `Client/Bin/ShaderFiles/Engine_Shader_*`를 건드릴 때는, 같은 턴 안에서 대상 파일과 Engine/Client counterpart를 먼저 읽어 원본 관계를 확인한 뒤에만 수정한다.
- `Clone(arg)`, `Initialize(arg)`, `Add_Component`, prototype/layer tag를 다룰 때는 구현 전에 arg 소비자와 key 소유자를 실제 코드로 추적한다.
- 기존 로직이나 helper, 도메인 support 파일과 겹치는 구현이 보이면, 중복 사실과 처리 방향을 먼저 보고한다. `Helper_*`는 범용 보조 성격에만 쓰고, 도메인 정본/metadata/schema/policy 성격이면 책임 이름을 우선한다.
- `DT_*.json`과 `DT_*.xlsx`는 직접 수정하지 않는다. DT 행 추가/수정이 필요하면 설명보다 먼저 Google Sheet 입력용 column 형태 표를 제시한다. 표는 실제 DT 컬럼명만 쓰고, 한국어 메모는 별도 `Id / Memo` 표로 분리한다. generated JSON은 사용자의 엑셀/테이블 export 흐름으로만 갱신한다.
- `UNREFERENCED_PARAMETER`는 사용하지 않는다. unused parameter 경고는 시그니처 정리, 구현 정리, 또는 더 직접적인 방식으로 해결한다.
- enum name/index/value 나열이 필요하면 수동 `switch`나 매핑 복제를 먼저 늘리지 말고 `magic_enum` 활용 가능성을 우선 검토한다. UI 표기 문자열이 enum 원문과 다르면 가장 좁은 범위의 local label ownership을 유지한다.
- 한 `.cpp` 안에서만 쓰는 구현 보조 함수·타입·상수·가변 상태는 `namespace Internal` 대신 무명 namespace에 둬서 internal linkage를 명시한다. 헤더에는 무명 namespace를 두지 않으며, 여러 번역 단위에서 의도적으로 참조해야 하는 선언은 역할이 드러나는 named namespace와 명시적인 linkage 계약을 사용한다.
- 헤더 선언은 기존 블록 순서를 따르고, `Create()`, `Clone()`, `Free()` 같은 factory/lifecycle 함수는 선언부 하단에 모은다.
- 개인 정리 기준이 필요한 C++ 헤더 구조/주석은 `docs/architecture/personal_cpp_header_guide.md`를 참고하되, 기존 팀 코드를 일괄 변경하는 근거로 삼지 않는다.
- 공용 헤더(`Engine/Public` 및 shared editor/core 헤더)에 새 public 함수를 추가하면 간략한 주석을 붙인다.
- 책임과 상수는 먼저 소유 클래스 또는 가장 좁은 범위에 둔다. namespace나 공통 helper로 올리는 것은 실제 반복과 의미 공통이 확인될 때만 한다.
- 새 C++ attribute는 기본적으로 쓰지 않는다. `[[deprecated]]`는 기존 API를 즉시 제거할 수 없고 새 경로로 명확히 유도해야 할 때만 예외 후보로 본다.

## 명명 규칙

- 새 코드와 새로 추가하는 식별자는 아래 기본 규칙을 따른다.
- 기존 명명 불일치는 자동 정리하지 않고, 요청되거나 승인된 범위에서만 수정한다.
- 팀 프로젝트에서는 주변 코드가 기준과 다를 수 있으므로 "주변 코드 스타일을 따른다"를 naming 기준으로 삼지 않는다.

| 대상 | 기본 규칙 |
| --- | --- |
| `struct`, `enum`, enum member, typedef | `UpperCamelCase` |
| `class` | `UpperCamelCase_UnderscoreTolerant` |
| `interface` | `IUpperCamelCase_UnderscoreTolerant` |
| 함수 | `UpperCamelCase_UnderscoreTolerant` |
| class 내부 상태/멤버 변수 | `_lowerCamelCase` |
| struct field, 지역 변수, 파라미터 | `lowerCamelCase` |
| class/global static const/constexpr | `kUpperCamelCase` |
| 전역 변수 | `gLowerCamelCase` |
| lambda, concepts | `all_lower` |
| 매크로 | `ALL_UPPER` |

## 문서 운영

- `AGENTS.md`는 짧은 규칙만 유지하고, 장문 아키텍처 설명이나 예외 사례는 별도 문서로 분리한다.
- 실행 계획과 작업 기록은 `docs/tasks`에서 상태 기준으로 운영한다: `01_planned`, `02_in-progress`, `03_completed`, `04_discarded`.
- `docs/tasks` 작업 문서는 `제목 -> 빈 줄 -> 문서 작성일/작업 시작일/작업 종료일 -> 빈 줄 -> 본문` 순서를 따른다.
- `/docs/`는 `.gitignore` 대상이라 문서 작성/수정 확인을 `git status`에 의존하지 말고 대상 파일을 직접 열어 확인한다.
- 상태 추적이 아닌 재조회용 참고/분석/학습 문서는 `docs/references`, 상황 메모/제안/handoff/보고서/안전 노트는 `docs/notes`, 퇴역 문서는 `docs/expired`에 둔다.
- 참고 문서의 부속 이미지/스크린샷 같은 비문서 리소스는 `docs/references/assets`에 둔다.

## 빌드 판단

- 빌드가 필요하면 저장소 루트에서 PowerShell로 실행한다.
- 솔루션 전체보다 변경을 소비하는 `*/Default/*.vcxproj`만 직접 빌드한다.
- `MSBuild.exe` 경로는 session-start hint 또는 `vswhere`로 확인하고 추측하지 않는다.
- 빌드 명령은 처음부터 권한 상승으로 실행한다. `Permission denied`/`LNK1104`는 우선 실행 중인 IDE·프로세스·환경 잠금으로 분류하며, 사용자 프로세스나 MainEditor를 종료하지 않는다.
- 기본 `IntDir`/`OutDir`을 사용하고 임의의 저장소 내 build 폴더를 만들지 않는다. 권한 상승 후에도 잠겨 검증이 막힌 경우에만 `.codex/hooks/managed_workspace.py`가 만든 `.codex/workspaces`의 관리형 slot을 사용한다.
- 관리형 slot에서는 Engine·Client·MainEditor의 프로젝트별 `IntDir`과 공용 `OutDir`을 함께 지정하고, 검증 종료 후 root를 정리한다. shared `.vcxproj`, configuration, Engine staging/post-build 구조는 빌드 편의만으로 바꾸지 않는다.
