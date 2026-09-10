#pragma once
#include "EffectRuntime_Types.h"
#include "Particle_Types.h"
#include "VIBufferCom_Instance.h"

NS_BEGIN(Engine)

class ComputeShaderCom;
class ComputeStructuredBuffer;

class ENGINE_DLL VIBufferCom_ComputePointParticle final : public VIBufferCom_Instance
{
    GENERATED_COMPONENT(VIBufferCom_ComputePointParticle);

public:
    VIBufferCom_ComputePointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBufferCom_ComputePointParticle(const VIBufferCom_ComputePointParticle& prototype);
    ~VIBufferCom_ComputePointParticle() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    // desc의 drawMode에 따라 direct 또는 compute-authored indirect draw를 제출
    HRESULT Render() override;

public: //## Behavior::ComputeUpdate
    // spawn/kill 요청을 반영한 뒤 instance payload를 갱신
    void Dispatch_Compute(
        float timeDelta,
        uint32 spawnRequest,
        uint32 spawnSerialBase,
        float lifetimeSamplePhase,
        float emitterElapsedTime,
        bool killActiveParticles);
    // 같은 instance count 안에서 위치/크기 runtime payload를 갱신
    void Update_RuntimeDesc(const ComputePointParticleDesc& desc);
    // emitter/world transform 변화가 compute spawn center에 반영되도록 갱신
    void Set_Center(const Vec3& center);
    // emitter local 값을 world 방향으로 바꾸는 basis를 갱신
    void Set_EmitterBasis(const Vec3& right, const Vec3& up, const Vec3& look);
    // effect 재사용 전 particle lifecycle/draw state를 초기화
    HRESULT Reset_ForEffectReplay();
    // CPU가 아는 범위에서 활성 particle이 있었는지 확인
    bool Has_ActiveParticles() const { return _hasActiveParticles; }
    // compute output에서 follower ribbon 대표 particle 위치를 읽기
    bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const;

private: //## Types::IndirectDraw
    // compute shader가 쓰고 indirect draw가 읽는 5-DWORD 공통 args payload
    struct ComputeIndirectArgs
    {
        uint32 value0{};
        uint32 instanceCount{};
        uint32 value2{};
        int32 value3{};
        uint32 startInstanceLocation{};
    };

    // compute shader가 particle별 수명 상태를 프레임 사이에 유지하기 위한 최소 payload
    struct ParticleLifecycleState
    {
        float lifeAge{};
        float lifeMax{};
        uint32 respawnSeed{};
        uint32 active{};
        uint32 placementIndex{};
        uint32 placementCount{};
        uint32 reserved0{};
        uint32 reserved1{};
    };

private: //## Static::ComputeDispatch
    // compute shader의 numthreads.x와 맞춘 dispatch 단위
    static constexpr uint32 kThreadCountX{ 64 };

private: //## Data::ParticleDesc
    // clone 생성 시 전달받는 compute particle 검증 규칙 저장
    ComputePointParticleDesc _desc{};

private: //## Data::Compute
    Shared<ComputeShaderCom> _computeShader{};               // instance payload를 쓰는 compute shader
    Shared<ComputeStructuredBuffer> _computeOutput{};        // compute 결과를 보관하는 structured UAV buffer
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};    // compute가 작성하는 indirect args source buffer
    Shared<ComputeStructuredBuffer> _lifecycleState{};       // particle별 age/lifetime을 유지하는 compute-only state buffer
    vector<ParticleLifecycleState> _initialLifecycleState{}; // replay reset 때 재사용하는 초기 lifecycle baseline
    vector<ParticleInstanceVertex> _emptyInstancePayload{};  // replay reset 때 재사용하는 zeroed instance payload
    float _elapsedTime{};                                    // shader에 전달할 emitter loop-local 시간
    float _deltaTime{};                                      // 이번 dispatch의 frame delta time
    float _lifetimeSamplePhase{};                            // 이번 spawn request에 적용할 Lifetime curve phase
    uint32 _spawnRequest{};                                  // 이번 dispatch에서 inactive slot을 활성화할 요청 수
    uint32 _spawnSerialBase{};                               // 이번 dispatch 첫 spawn의 replay-local 순번
    uint32 _killActiveParticles{};                           // 이번 dispatch에서 active particle을 모두 죽일지 여부
    Vec3 _sourceMotionVelocity{};                            // 이번 dispatch에서 사용할 emitter center delta 기반 source velocity
    Vec3 _previousSourceMotionCenter{};                      // 이전 dispatch의 emitter center
    bool _hasPreviousSourceMotionCenter{ false };            // source velocity 계산에 사용할 이전 center가 있는지 여부
    bool _sourceMotionVelocityValid{ false };                // 이번 dispatch source velocity가 유효한지 여부
    bool _hasActiveParticles{ false };                       // readback 없이 비활성 emitter submit을 줄이기 위한 CPU-side latch

private: //## Data::GPU
    D3D11_BUFFER_DESC _instanceBufferDesc{}; // compute output 복사 대상 instance VB 생성 설정
    ComPtr<Buffer> _computeConstantBuffer{}; // 시간/개수 등 frame parameter 전달용 constant buffer
    ComPtr<Buffer> _indirectArgsBuffer{};    // Draw*InstancedIndirect가 읽는 args destination buffer

private: //## Helper::Setup
    HRESULT Ready_VertexBuffer();        // prototype 공용 point vertex 1개를 생성
    HRESULT Ready_ComputeShader();       // compute-only shader를 clone 전용 실행 컴포넌트로 준비
    HRESULT Ready_ComputeOutput();       // ParticleInstanceVertex 배열을 쓰는 UAV buffer를 생성
    HRESULT Ready_LifecycleState();      // particle별 persistent lifetime state buffer를 생성
    HRESULT Ready_InstanceBuffer();      // DrawInstanced가 소비할 default instance VB를 생성
    HRESULT Ready_IndexedDrawBuffer();   // indexed indirect 검증용 1-index buffer를 생성
    HRESULT Ready_IndirectArgsBuffers(); // compute args source와 draw args destination buffer를 준비
    HRESULT Ready_ConstantBuffer();      // compute shader frame parameter용 constant buffer를 생성

private: //## Helper::Runtime
    HRESULT Update_ConstantBuffer();                                       // 누적 시간과 instance count를 compute shader에 전달
    HRESULT Reset_IndirectArgs();                                          // compute dispatch 전에 active/activation counters를 0으로 초기화
    HRESULT Copy_OutputToInstanceVB();                                     // compute output buffer를 instance VB로 복사
    HRESULT Copy_OutputToIndirectArgs();                                   // compute args source를 indirect args destination으로 복사
    HRESULT Ready_ResetBaseline();                                         // replay reset 때 재할당 없이 다시 쓸 초기 payload 준비
    ParticleLifecycleState Make_InitialLifecycleState(uint32 index) const; // 시작 시점의 particle별 수명 상태를 생성

public:
    static Shared<VIBufferCom_ComputePointParticle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
