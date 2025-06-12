// Fill out your copyright notice in the Description page of Project Settings.


#include "ABossCloneCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h" 

ABossCloneCharacter::ABossCloneCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ABossCloneCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (GEngine)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("clone spawn"));
    }

    PlayerActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->GravityScale = 1.0f;
    // 충돌 제거
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 기존처럼 0.5초 후 전투 진입하지 말고, 바로 진입
    EnterState(ECloneCombatState::CombatReady);
}



void ABossCloneCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TickCombatFSM(DeltaTime);

    // 공격 중에는 플레이어를 따라보지 않게 설정
    if (CurrentState != ECloneCombatState::Attacking)
    {
        FacePlayerSmoothly(DeltaTime);
    }
}


void ABossCloneCharacter::TickCombatFSM(float DeltaTime)
{
    // 공격 애니메이션 중엔 FSM 작동 정지
    if (GetMesh() && GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
    {
        return; // 애니메이션 끝날 때까지 기다림
    }

    if (!PlayerActor) return;

    switch (CurrentState)
    {
    case ECloneCombatState::Idle:
        break;

    case ECloneCombatState::CombatReady:
        EvaluateCombatReady();
        break;

    case ECloneCombatState::Attacking:
        // 공격 중 상태 유지
        break;

    case ECloneCombatState::Cooldown:
        // 대기
        break;
    }
}

void ABossCloneCharacter::EvaluateCombatReady()
{
    // 완전 랜덤 패턴 선택 (0~2)
    int32 RandomPattern = FMath::RandRange(0, 2);

    switch (RandomPattern)
    {
    case 0:
        CurrentPattern = ECloneAttackPattern::Claw;
        break;
    case 1:
        CurrentPattern = ECloneAttackPattern::Jump;
        break;
    case 2:
        CurrentPattern = ECloneAttackPattern::Dash;
        break;
    }

    SelectAndExecuteAttack();
}


void ABossCloneCharacter::SelectAndExecuteAttack()
{
    if (!PlayerActor) return;

    CachedAttackDirection = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    CachedAttackDirection.Z = 0.f;
    SetActorRotation(CachedAttackDirection.Rotation());

    EnterState(ECloneCombatState::Attacking); //  공격 시작 상태 설정

    switch (CurrentPattern)
    {
    case ECloneAttackPattern::Claw:
        PerformClawAttack();
        break;
    case ECloneAttackPattern::Jump:
        PerformJumpAttack();
        break;
    case ECloneAttackPattern::Dash:
        PerformDashAttack();
        break;
    }

    // EnterCooldown은 각 공격 함수 내부에서 처리함
}




void ABossCloneCharacter::EnterState(ECloneCombatState NewState)
{
    CurrentState = NewState;
}

void ABossCloneCharacter::EnterCooldown()
{
    EnterState(ECloneCombatState::Cooldown);

    FTimerHandle CDHandle;
    GetWorldTimerManager().SetTimer(CDHandle, [this]()
        {
            EnterState(ECloneCombatState::CombatReady);
        }, CooldownDuration, false);
}

void ABossCloneCharacter::FacePlayerSmoothly(float DeltaTime)
{
    if (!PlayerActor) return;

    FVector Dir = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FRotator LookAt = FRotationMatrix::MakeFromX(Dir).Rotator();

    LookAt.Pitch = 0.0f;
    LookAt.Roll = 0.0f;

    SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookAt, DeltaTime, 5.0f));
}


// === 공격들: 몽타주만 재생하고 실제 데미지는 없음 ===

void ABossCloneCharacter::PerformClawAttack()
{
    if (ClawMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* Anim = GetMesh()->GetAnimInstance();
        Anim->Montage_Play(ClawMontage);

        // 애니메이션 끝나면 Cooldown 상태로 전환
        FOnMontageEnded MontageEndDelegate;
        MontageEndDelegate.BindLambda([this](UAnimMontage* Montage, bool bInterrupted)
            {
                EnterCooldown();
            });

        Anim->Montage_SetEndDelegate(MontageEndDelegate, ClawMontage);

        // 이 타이밍에 Attacking 상태 설정
        EnterState(ECloneCombatState::Attacking);
    }
}


void ABossCloneCharacter::PerformDashAttack()
{
    if (!PlayerActor || !DashMontage) return;

    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (!Anim) return;

    Anim->Montage_Play(DashMontage);
    EnterState(ECloneCombatState::Attacking); // 공격 중 상태 설정

    // 몽타주 끝나면 쿨다운 전환
    FOnMontageEnded EndDelegate;
    EndDelegate.BindLambda([this](UAnimMontage*, bool) { EnterCooldown(); });
    Anim->Montage_SetEndDelegate(EndDelegate, DashMontage);

    // 이동 설정
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->BrakingFrictionFactor = 0.f;
        MoveComp->BrakingDecelerationWalking = 0.f;
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    }

    TWeakObjectPtr<ABossCloneCharacter> WeakThis(this);

    GetWorldTimerManager().SetTimer(DashStartTimer, FTimerDelegate::CreateLambda([WeakThis]()
        {
            if (!WeakThis.IsValid()) return;

            WeakThis->LaunchCharacter(WeakThis->CachedAttackDirection * 1200.0f, true, true);

            WeakThis->GetWorldTimerManager().SetTimer(WeakThis->MovementTimer, FTimerDelegate::CreateLambda([WeakThis]()
                {
                    if (!WeakThis.IsValid()) return;

                    if (WeakThis->GetMesh()->GetAnimInstance()->Montage_IsPlaying(WeakThis->DashMontage))
                    {
                        WeakThis->AddMovementInput(WeakThis->CachedAttackDirection, 1.0f);
                    }

                }), 0.02f, true);

            WeakThis->GetWorldTimerManager().SetTimer(WeakThis->RestoreTimer, FTimerDelegate::CreateLambda([WeakThis]()
                {
                    if (!WeakThis.IsValid()) return;

                    if (UCharacterMovementComponent* MoveComp = WeakThis->GetCharacterMovement())
                    {
                        MoveComp->BrakingFrictionFactor = 1.f;
                        MoveComp->BrakingDecelerationWalking = 2048.f;
                    }

                    if (UCapsuleComponent* Capsule = WeakThis->GetCapsuleComponent())
                    {
                        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                    }

                }), 1.0f, false);

        }), 1.9f, false);
}





void ABossCloneCharacter::PerformJumpAttack()
{
    if (!PlayerActor || !JumpMontage) return;

    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    Anim->Montage_Play(JumpMontage);
    EnterState(ECloneCombatState::Attacking);

    FOnMontageEnded EndDelegate;
    EndDelegate.BindLambda([this](UAnimMontage*, bool) { EnterCooldown(); });
    Anim->Montage_SetEndDelegate(EndDelegate, JumpMontage);

    // 타이머 기반 점프 동작은 그대로 유지
    FTimerHandle JumpDelayTimer;
    GetWorldTimerManager().SetTimer(JumpDelayTimer, [this]()
        {
            FVector LaunchVelocity = CachedAttackDirection * 600.f + FVector(0, 0, 600.f);
            LaunchCharacter(LaunchVelocity, true, true);
        }, 0.9f, false);
}



