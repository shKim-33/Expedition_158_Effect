#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ComputeShaderCom;
class ComputeStructuredBuffer;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

// Trail sample provider history를 GPU strip payload로 낮춰 trail을 렌더
class ComputeTrailEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeTrailEmitter, ObjectType::EffectEmitter);

public:
    ComputeTrailEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeTrailEmitter(const ComputeTrailEmitter& prototype);
    ~ComputeTrailEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    // effect root 재사용 시 trail history와 draw runtime을 초기화
    HRESULT Reset_ForEffectReplay() override;
    // trail은 일단 자동 완료가 아니라 명시 Stop/release를 따름
    bool Is_EffectFinished() const override;
    // acquire 시점 owner가 준 trail sample provider를 현재 runtime desc에 다시 바인딩
    void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider) override;

private: //## Types::TrailRuntime
    struct TrailHistorySample
    {
        EffectTrailSample sample{}; // provider에서 받은 base/tip world sample
        float age{};                // sample이 누적된 뒤 흐른 시간
        uint32 serial{};            // 생성 시점 material random seed용 stable key
    };

    struct TrailRenderSample
    {
        EffectTrailSample sample{}; // render 직전에 보간된 base/tip world sample
        float age{};                // 보간된 sample의 age
        float distanceFromHead{};   // head sample 기준 누적 trail 거리. UV 거리 계산에 사용
        uint32 serial{};            // source history sample에서 이어받은 stable key
    };

    struct TrailSamplePayload
    {
        Vec4 baseWorldPosition{}; // compute shader가 읽을 trail base world position
        Vec4 tipWorldPosition{};  // compute shader가 읽을 trail tip world position
        Vec4 sampleParams{};      // x: sample age, y: head 기준 누적 거리, zw: reserved
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct TrailComputeParams
    {
        uint32 segmentCount{};           // 이번 frame에 유효한 trail segment 수
        float segmentLifetime{};         // age fade 기준 lifetime
        float width{};                   // base/tip span에 곱할 strip 폭 비율
        float uvTiling{};                // trail noise/breakup 길이 방향 UV 반복 배율
        float sideFade{};                // 폭 방향 alpha edge 감소 강도
        Vec3 padding{};                  // constant buffer 16-byte 정렬 유지용 padding
        Vec4 startColor{};               // InitialColor 모듈에서 낮춘 segment 시작 색상
        Vec4 endColor{};                 // ColorOverLife 모듈에서 낮춘 segment 끝 색상
        Vec4 colorOverLifeCurveParams{}; // x: color key count, y: alpha key count, z/w: enabled flags.
        Vec4 colorOverLifeColorCurveTimes{};
        Vec4 colorOverLifeColorCurveTimesBlock1{};
        Vec4 colorOverLifeColorCurveValuesR{};
        Vec4 colorOverLifeColorCurveValuesRBlock1{};
        Vec4 colorOverLifeColorCurveValuesG{};
        Vec4 colorOverLifeColorCurveValuesGBlock1{};
        Vec4 colorOverLifeColorCurveValuesB{};
        Vec4 colorOverLifeColorCurveValuesBBlock1{};
        Vec4 colorOverLifeColorCurveArriveR{};
        Vec4 colorOverLifeColorCurveArriveRBlock1{};
        Vec4 colorOverLifeColorCurveArriveG{};
        Vec4 colorOverLifeColorCurveArriveGBlock1{};
        Vec4 colorOverLifeColorCurveArriveB{};
        Vec4 colorOverLifeColorCurveArriveBBlock1{};
        Vec4 colorOverLifeColorCurveLeaveR{};
        Vec4 colorOverLifeColorCurveLeaveRBlock1{};
        Vec4 colorOverLifeColorCurveLeaveG{};
        Vec4 colorOverLifeColorCurveLeaveGBlock1{};
        Vec4 colorOverLifeColorCurveLeaveB{};
        Vec4 colorOverLifeColorCurveLeaveBBlock1{};
        Vec4 colorOverLifeColorCurveModes{};
        Vec4 colorOverLifeColorCurveModesBlock1{};
        Vec4 colorOverLifeAlphaCurveTimes{};
        Vec4 colorOverLifeAlphaCurveTimesBlock1{};
        Vec4 colorOverLifeAlphaCurveValues{};
        Vec4 colorOverLifeAlphaCurveValuesBlock1{};
        Vec4 colorOverLifeAlphaCurveArrive{};
        Vec4 colorOverLifeAlphaCurveArriveBlock1{};
        Vec4 colorOverLifeAlphaCurveLeave{};
        Vec4 colorOverLifeAlphaCurveLeaveBlock1{};
        Vec4 colorOverLifeAlphaCurveModes{};
        Vec4 colorOverLifeAlphaCurveModesBlock1{};
        Vec4 sizeByLifeParams{};         // x/y: width start/end, z: enabled, w: reserved.
        Vec4 sizeByLifeCurveTimes{};     // SizeByLife scaleOverLife key times
        Vec4 sizeByLifeCurveTimesBlock1{};
        Vec4 sizeByLifeCurveValuesX{};   // SizeByLife scaleOverLife X multiplier 값
        Vec4 sizeByLifeCurveValuesXBlock1{};
        Vec4 sizeByLifeCurveArriveTangentsX{};
        Vec4 sizeByLifeCurveArriveTangentsXBlock1{};
        Vec4 sizeByLifeCurveLeaveTangentsX{};
        Vec4 sizeByLifeCurveLeaveTangentsXBlock1{};
        Vec4 sizeByLifeCurveModes{};
        Vec4 sizeByLifeCurveModesBlock1{};
        Vec4 sizeByLifeCurveParams{};    // x: key count, y: multiplyX, zw: reserved.
        Vec4 lengthParams{};             // x: maxTrailLength, y: tailFadeLength, z: autoLifeFade flag, w: reserved.
    };

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{}; // DrawIndexedInstancedIndirect의 index count
        uint32 instanceCount{};         // compute가 작성하는 실제 draw segment 수
        uint32 startIndexLocation{};    // 단일 point index 시작 위치
        int32 baseVertexLocation{};     // 단일 point vertex 기준 base offset
        uint32 startInstanceLocation{}; // instance 시작 위치
    };

private: //## Static::Runtime
    // trail segment draw shader id
    static constexpr auto kTrailShaderId{ "Shader_EffectTrail" };
    // SpriteDistortion trail segment draw shader id
    static constexpr auto kTrailDistortionShaderId{ "Shader_EffectTrailDistortion" };
    // render sample을 instance payload로 낮추는 compute shader id
    static constexpr auto kTrailComputeShaderId{ "ComputeShader_TrailStrip" };
    // texture 미지정 시 사용할 기본 텍스처
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    // compute shader numthreads.x와 맞춘 dispatch 단위
    static constexpr uint32 kThreadCountX{ 64 };
    // preview 폭주를 막는 interval당 render sample 상한
    static constexpr uint32 kMaxCurveSubdivision{ 32 };
    // 한 frame에서 SpawnPerUnit이 추가할 수 있는 provider history sample 상한
    static constexpr uint32 kMaxSpawnPerUnitFrameSamples{ 64 };
    // tangent smoothing용 양옆 sample 가중치
    static constexpr float kSmoothSampleSideWeight{ 0.25f };
    // main texture alpha를 opacity로 사용
    static constexpr int kOpacitySourceAlpha{ 0 };
    // main texture red를 opacity로 사용
    static constexpr int kOpacitySourceRed{ 1 };
    // main texture luminance를 opacity로 사용
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::RuntimeDesc
    // clone 시점에 받은 trail runtime 설정
    ComputeTrailEmitterDesc _desc{};

private: //## Data::TrailRuntime
    // CPU가 보관하는 provider sample history
    vector<TrailHistorySample> _history{};
    // 이번 frame에 그릴 segment 개수
    uint32 _drawSegmentCount{};
    // material UV scroll 계산에 사용할 emitter-local 누적 시간
    float _materialElapsedTime{};
    // body/mask UV stretch에 사용할 현재 표시 trail 길
    float _visibleTrailLength{ 1.f };
    // material particle-life uniform random에 사용할 sample serial counter
    uint32 _sampleSerialCounter{};

private: //## Data::Components
    // trail draw shader
    Shared<ShaderCom> _shader{};
    // trail payload 생성 compute shader
    Shared<ComputeShaderCom> _computeShader{};
    // trail main texture
    Shared<Texture> _mainTexture{};
    // 선택 noise texture. 없으면 material noise를 비활성화
    Shared<Texture> _noiseTexture{};
    // 선택 mask texture. 없으면 mask alpha 소비를 비활성화
    Shared<Texture> _maskTexture{};
    // 선택 distortion flow texture. 없으면 distortion offset을 0으로 처리
    Shared<Texture> _flowTexture{};
    // instanced point draw용 단일 vertex buffer
    ComPtr<Buffer> _pointVB{};
    // instanced point draw용 단일 index buffer
    ComPtr<Buffer> _indexBuffer{};
    // compute 결과를 복사해 draw가 읽는 instance buffer
    ComPtr<Buffer> _instanceBuffer{};
    // compute shader frame parameter buffer
    ComPtr<Buffer> _computeConstantBuffer{};
    // DrawIndexedInstancedIndirect가 읽는 args destination buffer
    ComPtr<Buffer> _indirectArgsBuffer{};
    // CPU render sample을 compute가 읽는 SRV buffer
    Shared<ComputeStructuredBuffer> _historyInput{};
    // compute가 작성하는 TrailInstanceVertex buffer
    Shared<ComputeStructuredBuffer> _computeOutput{};
    // compute가 작성하는 indirect args source buffer
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_NoiseTexture();
    HRESULT Ready_MaskTexture();
    HRESULT Ready_FlowTexture();
    HRESULT Ready_DrawBuffers();
    HRESULT Ready_ComputeBuffers();

private: //## Helper::TrailRuntime
    // provider sample을 history에 누적하고 수명/개수를 정리
    void Update_History(float timeDelta);
    // segment lifetime 안쪽의 visible history sample 수
    uint32 Count_VisibleHistorySamples() const;
    // visible sample을 우선 보존하고 tail control-only sample 하나만 기록
    void Prune_HistoryForVisibleControl();
    // render-time tessellation을 포함한 최대 segment capacity
    uint32 Compute_MaxRenderSegmentCount() const;
    // SpawnPerUnit 정책에 따라 provider sample과 보강 sample을 history에 삽입
    void Insert_TrailSamplesForFrame(const EffectTrailSample& trailSample);
    // provider/render 보강 sample을 history head에 삽입
    void Insert_TrailSample(const EffectTrailSample& trailSample, float age);
    // 원본 history를 render-time curve sample 배열로 변환
    void Build_RenderSamples(vector<TrailRenderSample>& outSamples) const;
    // smoothTangent 옵션에 따라 history sample을 완만하게 보결정
    EffectTrailSample Build_SmoothedHistorySample(uint32 sampleIndex) const;

private: //## Helper::Render
    // render sample을 trail instance payload로 낮추는 compute pass를 실행
    HRESULT Dispatch_Compute();
    // CPU render sample을 compute 입력 structured buffer에 업로드
    HRESULT Update_ComputeInput();
    HRESULT Update_ComputeConstants();
    // indirect draw args를 draw count 0 상태로 초기화
    HRESULT Reset_IndirectArgs();
    // compute 결과 instance/args buffer를 draw용 buffer로 복사
    HRESULT Copy_ComputeOutput();
    HRESULT Bind_ShaderResources();
    HRESULT Bind_DistortionResources();
    // replay 단위 Lifetime random을 segment lifetime window로 변환
    void Resample_SegmentLifetime();
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const char* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    // authoring opacitySource 문자열을 shader selector 값으로 변환
    int Resolve_OpacitySourceIndex() const;
    uint32 Resolve_BlendPassIndex() const;

public:
    static Shared<ComputeTrailEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
