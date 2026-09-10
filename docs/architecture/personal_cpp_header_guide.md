# Personal C++ Header Guide

문서 작성일: 2026-06-19
주석 규칙 추가: 2026-09-11

이 문서는 기존 팀 코드를 일괄 변경하자는 기준이 아니다.
Codex와 내가 새 C++ 헤더를 작성하거나, 승인된 정리 작업에서 헤더 구조와 주석을 맞출 때 참고하는 개인 선호 규칙이다.

## 기본 도식

```cpp
// [1] 클래스 상단: 이 클래스의 책임을 한 문장으로 설명한다.
class XxxEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(...)

public: //## Types::Public
    // [2] 외부에서 직접 소비하는 public enum/struct/using은 매크로 바로 아래에 둔다.
    struct PublicDesc
    {
        int count{};      // enum, struct 등의 field 설명은 우측 trailing comment.
        float duration{}; // 짧은 payload 설명에 적합.
    };

public:
    // [3] 기본 구조 함수/엔진 lifecycle override는 보통 태그 없이 둔다.
    XxxEmitter(...);
    XxxEmitter(...);
    ~XxxEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    HRESULT Render() override;

public: //## Behavior::Something
    // [4] 클래스별 의미 있는 공개 동작은 태그로 묶는다.
    // 함수 설명은 선언 바로 위에 둔다.
    HRESULT Do_Something();

public: //## Accessors
    // [5] 조회성 public 함수는 Accessors로 묶는다.
    int Get_Value() const;

protected: //## Data::Something
    // [6] protected 멤버 변수도 함수보다 먼저 둔다.
    bool _ready{ false };

protected: //## Hook
    // [7] override hook도 독립 의미가 있으면 태그로 묶는다.
    void On_Something() override;

private: //## Types::Something
    // [8] 내부 enum/struct/using 묶음.
    enum class State : uint8
    {
        ...
    };

private: //## Static Values
    // [9] static constexpr, inline static 값 묶음.
    static constexpr int kSomething{ 1 };

private: //## Data::Something
    // [10] 멤버 변수 설명은 변수 선언 위에 둔다.
    int _value{};

private: //## Helper::Something
    // [11] 내부 helper 함수 묶음.
    HRESULT Ready_Something();

public:
    // [12] Create / Clone / Free 같은 factory/lifecycle 꼬리 함수는 기존 관례대로 하단에 모은다.
    static Shared<XxxEmitter> Create(...);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

struct XxxRuntimeDesc
{
    int count{};        // [13] struct field 설명은 우측 trailing comment.
    float duration{};   // 짧은 payload 설명에 적합.
    Vec3 position{};    // 정렬 cleanup 대상이 되어도 구조체 내부라 감수 가능.
};
```

## 구조 적용 원칙

- `//##` 태그는 헤더 스캔용 지도다. 기본 구조 함수에는 과하게 붙이지 않는다.
- 접근 지정자(`public`, `protected`, `private`) 각각의 내부 기본 선언 순서는 `Types(enum/struct/using) -> Static Values/Constants -> Data/Variables -> Functions`다.
- 선언 순서가 별도로 필요한 예외가 아니라면, 태그 순서도 위 기본 선언 순서를 우선한다. 예를 들어 `protected` 안에서는 `Data`가 `Hook`보다 먼저 온다.
- 전체 클래스 흐름은 대체로 `Types::Public -> 기본 public 함수 -> Behavior -> Accessors -> protected/private Types -> Static Values -> Data -> Helper -> factory tail`을 따른다.
- public enum/struct/using은 `Types::Public`으로 묶고 `GENERATED_*` 매크로 바로 아래에 둔다.

## 주석 규칙

### 무엇을 남기는가

판정 기준은 "반복인가"가 아니라 **"이 파일을 처음 여는 사람이 이름과 타입만으로 복원할 수 있는가"** 다. 만든 사람 기준으로 판단하면 `*Block1` 같은 것이 반복이라는 이유로 잘려나간다.

- **남긴다**: 좌표계 해석, 패킹 규칙, 예외 조건, 정책 선택, 단위, 수명·소유권 계약, 파일 밖 불변식(HLSL `numthreads`, 직렬화 포맷), 호출 순서 제약, 부수효과
- **남긴다**: 이름이 동작을 배신하는 경우. `...SpeedScale` 인데 실제로는 더하는 값이면 반드시 적는다
- **지운다**: `Get_*` / `Ready_*` / `Bind_*` / `Is_*` 앞의 이름 반복 서술, 멤버 이름을 되풀이하는 trailing 주석
- **애매하면 남긴다.** 지운 주석은 되돌릴 원본이 없다

### 반복은 섹션 주석으로 접는다

여러 필드가 공유하는 규칙은 개별 trailing 대신 한 줄로 묶는다. 이때 **적용 대상을 이름으로 명시**한다. 빈 줄로만 경계를 잡으면 나중에 필드가 끼어들 때 조용히 틀려진다.

```cpp
// accelerationCurve* : 커브 키를 Vec4 두 개에 나눠 저장. 기본 필드가 키 0~3, *Block1 이 키 4~7 (최대 8키)
```

여러 줄을 접은 대가로 남기는 한 줄이므로, 이유까지 적어 친절하게 쓴다.

### 위치

| 선언 종류 | 위치 |
|---|---|
| 타입 (`enum class` / `struct` / `class`) | 선언 위 |
| 함수 | 선언 위 |
| `struct` field, enum 값 | 우측 trailing |
| `class` 멤버 변수 | 선언 위 |

`class` 멤버와 함수를 위에 두는 이유는 정렬 cleanup 때 태그나 선언 주석이 같이 흔들리는 일을 줄이기 위해서다. `struct` field는 payload 값 설명이라 필드와 한 줄로 대응되는 편이 읽기 좋다.

예외: 선언이 여러 줄로 열리거나(`vector<T> keys{` 처럼) 100자를 넘으면 우측에 자리가 없으므로 위로 올린다.

### 표현

- 종결은 **명사형**으로 통일한다. 마침표는 붙이지 않는다.
- `X한다` 는 `X` 로 줄인다. 불규칙 용언은 자연스러운 명사로 옮긴다 — `고른다 -> 선택`, `그린다 -> 렌더`, `민다 -> 이동`. `-ㅁ/음` 을 기계적으로 붙이면 `고름 / 그림 / 밂` 처럼 다른 뜻과 겹치거나 표기가 낯설어진다.
- `있다 / 없다` 는 받침이 `ㅆ / ㅄ` 이라 명사로 오인하기 쉽다. `있음 / 없음` 으로 쓴다.
- 길이가 문제면 어미가 아니라 **되풀이되는 맥락**을 걷어낸다. 어미를 잘라야 2~3자지만 반복되는 주어를 걷어내면 절반이 준다.
- doxygen(`///`, `@brief`, `@note`)은 쓰지 않는다. 문서 생성을 돌리지 않으므로 태그가 정보를 더하지 않는다.

### 열 정렬

- 그룹 경계는 빈 줄, 접근 지정자, 타입 선언, 중괄호 단독 줄이다.
- 그룹 안 모든 trailing 주석을 **들여쓰기와 무관하게** 한 열에 맞춘다. 정렬 열은 그룹 내 최장 코드 + 1이다.
- 상한은 **140칸**이다. 에디터 래핑 설정과 같은 값이다.
- 140칸을 넘으면 길이가 비슷한 하위 묶음으로 나눈다. 단 파일 순서상 줄이 엇갈려 지그재그가 되면 나누지 않고 한 열을 유지하고, 넘치는 줄은 예외로 둔다.
- 한 줄로 충분한 선언을 열 맞추려고 쪼개지 않는다. 선언이 원래 여러 줄이어야 하는 경우에만 `};` 에 주석을 단다.

> ReSharper 의 주석 정렬은 *같은 들여쓰기로 연속된 줄* 만 한 묶음으로 본다. 선언이 여러 줄로 걸치고 주석이 `};` continuation 에 붙는 형태에서 묶음이 갈라져 한 struct 안에 열이 여러 종류로 남는다. 이 저장소에는 그런 형태가 많아 ReSharper 정렬에 의존하지 않는다.

### 구획

- 필드 **6개 이상** 묶음은 `#pragma region <이름>` / `#pragma endregion` 으로 감싼다. 30필드짜리 패킹 블록은 접을 수 있어야 값이 크다.
- 6개 미만은 빈 줄로만 띄운다. region 두 줄이 오히려 손해다.
- 저장소 관례를 따른다: 열 0, 앞에 빈 줄, `endregion` 은 마지막 멤버 뒤. (`Engine/Public/Sound_Manager.h`, `Client/Public/Client_Constants.h` 참조)
- region 이 이름을 맡으므로 그 안 섹션 주석에서는 접두사를 되풀이하지 않는다.

## 공통

- 기존 주석 내용이 틀리지 않았다면 내용 재작성보다 위치, 분류, 정렬 개선을 우선한다.
- 정리 작업은 **주석을 제외한 코드가 바이트 단위로 동일한지** 확인하며 진행한다. 이 검사 하나가 코드가 주석 처리되거나 변형되는 사고를 다 잡는다.
- 기존 팀 코드에 이 기준을 소급 적용하지 않는다. 새 코드나 명시적으로 승인된 헤더 정리 범위에서만 참고한다.
