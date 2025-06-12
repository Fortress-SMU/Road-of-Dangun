// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ABossCloneCharacter.generated.h"

UENUM(BlueprintType)
enum class ECloneCombatState
{
    Idle,
    CombatReady,
    Attacking,
    Cooldown
};

UENUM(BlueprintType)
enum class ECloneAttackPattern
{
    Claw,
    Jump,
    Dash
};

UCLASS()
class RoadOfDangun_API ABossCloneCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABossCloneCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void TickCombatFSM(float DeltaTime);
    void EvaluateCombatReady();
    void SelectAndExecuteAttack();

    void PerformClawAttack();
    void PerformDashAttack();
    void PerformJumpAttack();

    void EnterState(ECloneCombatState NewState);
    void EnterCooldown();

    FVector CachedAttackDirection;

    // Clone 공격 타이머들
    FTimerHandle DashStartTimer;
    FTimerHandle MovementTimer;
    FTimerHandle RestoreTimer;


    void FacePlayerSmoothly(float DeltaTime);

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* ClawMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* DashMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* JumpMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float CooldownDuration = 1.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    ECloneCombatState CurrentState = ECloneCombatState::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    ECloneAttackPattern CurrentPattern = ECloneAttackPattern::Claw;

    UPROPERTY()
    AActor* PlayerActor;
};