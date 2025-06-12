// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ABossCloneCharacter.h"
#include "TigerBossCharacter.generated.h"



UENUM(BlueprintType)
enum class EBossPhase : uint8
{
    Phase1,
    Phase2
};

UENUM(BlueprintType)
enum class EAttackPattern : uint8
{
    Idle,
    Claw,
    Jump,
    Dash,
    TeleportAndDash,
    CloneSpawn,
    Roar,
    MultiDash,
    WideClaw
};

UENUM(BlueprintType)
enum class EBossCombatState : uint8
{
    Idle,
    Approaching,
    Repositioning,
    CombatReady,
    Attacking,
    Cooldown,
    Staggered,
    RotatingToAttack
};

// 델리게이트 선언: float 매개변수 하나 (ex. 새로운 체력값)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHP, float, MaxHP);

UCLASS()
class RoadOfDangun_API ATigerBossCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATigerBossCharacter();
    void PerformClawAttack();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Attack")
    bool bCanAttack;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChanged OnHealthChanged;
    UFUNCTION(BlueprintCallable)
    void callDelegate(float currentHP, float MaxHP);
    UFUNCTION()
    void TestPrint_Two(float currentHP, float MaxHP);



protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 상태
    EBossPhase CurrentPhase;
    EAttackPattern CurrentPattern;


    // 체력
    UFUNCTION()
    void HandleHealthChanged(float CurrentHP, float MaxHP);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CachedCurrentHp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float CachedMaxHp;

    // TigerBossCharacter.h
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
    float PercentHP;
    float CurrentHealth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
    bool bIsDead = false;

    UFUNCTION(BlueprintCallable)
    void Dead();


    //분신관리
    UPROPERTY()
    TArray<TWeakObjectPtr<ABossCloneCharacter>> ActiveClones;

    // 보이스 사운드들
    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Entry;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Claw;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Jump;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* Voice_Dash;



    // 쿨다운
    
    float AttackCooldown;
    FTimerHandle AttackCooldownTimer;
    FTimerHandle WaitTimer;
    FTimerHandle DashStartTimer;
    FTimerHandle MovementTimer;
    FTimerHandle CollisionTimer;



    // 공격
    bool bIsInCombo;
    int32 ComboStep;
    float TimeSinceLastCombat = 0.f;
    void TryAttack();

    //블프이벤트노드
    UFUNCTION(BlueprintImplementableEvent, category = "MyFunctions")
    void CreateAttackTraceCPP();

    UFUNCTION(BlueprintImplementableEvent, category = "MyFunctions")
    void StopAttackTraceCPP();


    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* ClawMontage;

    UFUNCTION(BlueprintCallable)
    void HandleClawDamageNotify();
    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* JumpMontage;

    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* DashMontage;
    EBossCombatState CombatState;

    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* EvadeMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DeathMontage;


    //분신
    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    TSubclassOf<AActor> CloneClass;

    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    int32 NumClones = 3;

    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    float CloneRadius = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Boss|Clone")
    float CloneLifetime = 4.0f;



    // 거리 유지 / 회피
    UPROPERTY(EditAnywhere, Category = "Boss AI")
    float DesiredDistanceFromPlayer;

    UPROPERTY(EditAnywhere, Category = "Boss AI")
    float EvasionProbability;

    UPROPERTY(EditAnywhere, Category = "Boss AI")
    float EvasionDistance;

    APawn* PlayerActor;

    // 출현 연출용 변수
    bool bIsFalling = false;
    float FallElapsed = 0.f;
    FVector FallStartLocation;
    FVector FallTargetLocation;

    // 출현 애니메이션
    UPROPERTY(EditAnywhere, Category = "Boss|Animation")
    UAnimMontage* EntryMontage;

    // 출현 연출용 함수
    void StartFallSequence();


    // 행동 로직
    void DecideAndExecuteAttack();
    void CheckPhaseTransition();
    void SelectAndExecuteAttack();
    void ResetAttack();
    void RotateTowardPlayerThen(TFunction<void()> Callback);

    FTimerHandle RotateCheckTimerHandle;
    FTimerHandle AirHitTimer;


    // 이동 행동
    void SlowApproach();
    void Retreat();
    void FastCharge();
    void CircleAroundPlayer();

    // 공격 패턴
    void PerformJumpAttack();
    void PerformDashAttack();
    void StartDashSequence(); // 돌진 본체
    void PerformTeleportAndDash();
    void PerformCloneSpawn();
    void PerformRoar();
    void PerformMultiDash();
    void PerformWideClaw();
    void StartComboPattern();
    void ExecuteComboStep();

    void TickCombatFSM(float DeltaTime);
    void EnterState(EBossCombatState NewState);
    void EvaluateCombatReady();
    void EnterCombatReady();

    //회전
    bool bWantsToRotateToPlayer = false;
    FRotator TargetRotationForAttack;
    TFunction<void()> PostRotationAction; // 회전 후 실행할 함수



};
