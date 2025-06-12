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
    // �浹 ����
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // ����ó�� 0.5�� �� ���� �������� ����, �ٷ� ����
    EnterState(ECloneCombatState::CombatReady);
}



void ABossCloneCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TickCombatFSM(DeltaTime);

    // ���� �߿��� �÷��̾ ������ �ʰ� ����
    if (CurrentState != ECloneCombatState::Attacking)
    {
        FacePlayerSmoothly(DeltaTime);
    }
}


void ABossCloneCharacter::TickCombatFSM(float DeltaTime)
{
    // ���� �ִϸ��̼� �߿� FSM �۵� ����
    if (GetMesh() && GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
    {
        return; // �ִϸ��̼� ���� ������ ��ٸ�
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
        // ���� �� ���� ����
        break;

    case ECloneCombatState::Cooldown:
        // ���
        break;
    }
}

void ABossCloneCharacter::EvaluateCombatReady()
{
    // ���� ���� ���� ���� (0~2)
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

    EnterState(ECloneCombatState::Attacking); //  ���� ���� ���� ����

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

    // EnterCooldown�� �� ���� �Լ� ���ο��� ó����
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


// === ���ݵ�: ��Ÿ�ָ� ����ϰ� ���� �������� ���� ===

void ABossCloneCharacter::PerformClawAttack()
{
    if (ClawMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* Anim = GetMesh()->GetAnimInstance();
        Anim->Montage_Play(ClawMontage);

        // �ִϸ��̼� ������ Cooldown ���·� ��ȯ
        FOnMontageEnded MontageEndDelegate;
        MontageEndDelegate.BindLambda([this](UAnimMontage* Montage, bool bInterrupted)
            {
                EnterCooldown();
            });

        Anim->Montage_SetEndDelegate(MontageEndDelegate, ClawMontage);

        // �� Ÿ�ֿ̹� Attacking ���� ����
        EnterState(ECloneCombatState::Attacking);
    }
}


void ABossCloneCharacter::PerformDashAttack()
{
    if (!PlayerActor || !DashMontage) return;

    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (!Anim) return;

    Anim->Montage_Play(DashMontage);
    EnterState(ECloneCombatState::Attacking); // ���� �� ���� ����

    // ��Ÿ�� ������ ��ٿ� ��ȯ
    FOnMontageEnded EndDelegate;
    EndDelegate.BindLambda([this](UAnimMontage*, bool) { EnterCooldown(); });
    Anim->Montage_SetEndDelegate(EndDelegate, DashMontage);

    // �̵� ����
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

    // Ÿ�̸� ��� ���� ������ �״�� ����
    FTimerHandle JumpDelayTimer;
    GetWorldTimerManager().SetTimer(JumpDelayTimer, [this]()
        {
            FVector LaunchVelocity = CachedAttackDirection * 600.f + FVector(0, 0, 600.f);
            LaunchCharacter(LaunchVelocity, true, true);
        }, 0.9f, false);
}



