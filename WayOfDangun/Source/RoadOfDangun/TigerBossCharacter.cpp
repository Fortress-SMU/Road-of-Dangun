#include "TigerBossCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ABossCloneCharacter.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"



ATigerBossCharacter::ATigerBossCharacter()
{
    PrimaryActorTick.bCanEverTick = true;


    DesiredDistanceFromPlayer = 600.0f;

    bCanAttack = true;
    bIsInCombo = false;
    ComboStep = 0;

    CurrentPhase = EBossPhase::Phase1;
    CombatState = EBossCombatState::Idle;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ATigerBossCharacter::BeginPlay()
{
    Super::BeginPlay();
    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("C++ BeginPlay called"));

    // �÷��̾� ����
    PlayerActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    CombatState = EBossCombatState::Idle;
    bCanAttack = false;
    bIsInCombo = false;
    ComboStep = 0;


    // ���� ���� ��ġ ����
    FallStartLocation = GetActorLocation();

    // �ٴ� ã�� (���� Ʈ���̽�)
    FVector Start = FallStartLocation;
    FVector End = Start - FVector(0, 0, 10000.0f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        float CapsuleHalf = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FallTargetLocation = Hit.ImpactPoint + FVector(0, 0, CapsuleHalf + 1.f);
    }
    else
    {
        FallTargetLocation = FallStartLocation - FVector(0, 0, 300.0f);
    }

    // �߷� ����
    GetCharacterMovement()->SetMovementMode(MOVE_None);

    // ���� �ִϸ��̼� ���
    if (EntryMontage && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->Montage_Play(EntryMontage);

        FTimerHandle VoiceDelayHandle;
        GetWorldTimerManager().SetTimer(VoiceDelayHandle, [this]()
            {
                if (Voice_Claw)
                {
                    UGameplayStatics::PlaySoundAtLocation(this, Voice_Claw, GetActorLocation());
                }
            }, 1.0f, false);
    }

    bIsFalling = true;
    FallElapsed = 0.f;

}





void ATigerBossCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);


    if (bIsFalling)
    {
        FVector CurrentLocation = GetActorLocation();
        FVector ToTarget = FallTargetLocation - CurrentLocation;
        float DistanceToTarget = ToTarget.Size();

        if (DistanceToTarget < 2.0f)
        {
            bIsFalling = false;

            // ��ġ ���� ����
            TeleportTo(FallTargetLocation, GetActorRotation(), false, true);

            GetCharacterMovement()->SetMovementMode(MOVE_Walking);

            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::StartFallSequence, 2.4f, false);
            return;
        }

        float FallDuration = 0.6f;
        float FallSpeed = (FallStartLocation - FallTargetLocation).Size() / FallDuration;
        FVector MoveDir = ToTarget.GetSafeNormal();
        FVector NewLocation = CurrentLocation + MoveDir * FallSpeed * DeltaTime;

        TeleportTo(NewLocation, GetActorRotation(), false, true);
        return;
    }

    if (CurrentHealth <= 0.f && !bIsDead)
    {
        Dead();
        return;
    }

    //  ���ϰ� ���� ��쿡�� FSM ����
    CheckPhaseTransition();
    TickCombatFSM(DeltaTime);

    // ȸ�� ó��
    if (bWantsToRotateToPlayer)
    {
        FRotator Current = GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(Current, TargetRotationForAttack, DeltaTime, 12.0f);
        SetActorRotation(NewRot);

        float DeltaYaw = FRotator::NormalizeAxis(NewRot.Yaw - TargetRotationForAttack.Yaw);
        if (FMath::Abs(DeltaYaw) < 1.0f)
        {
            bWantsToRotateToPlayer = false;
            if (PostRotationAction)
            {
                PostRotationAction();
                PostRotationAction = nullptr;
            }
        }
    }
}



void ATigerBossCharacter::CheckPhaseTransition()
{
    if (CurrentPhase == EBossPhase::Phase1 && PercentHP <= 0.5f)
    {
        CurrentPhase = EBossPhase::Phase2;
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Phase 2 ����!"));
        // PerformRoar(); �� �߰� ����
    }
}


void ATigerBossCharacter::TickCombatFSM(float DeltaTime)
{
    if (bIsDead) return;  // �׾����� FSM ��Ȱ��ȭ
    if (!PlayerActor) return;
    if (!GetMesh() || !GetMesh()->GetAnimInstance()) return;
    // ���� ���� �� ȸ�� ����
    if (CombatState != EBossCombatState::Attacking &&
        CombatState != EBossCombatState::Cooldown &&
        GetMesh() && !GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
    {
        FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
        FRotator LookAtRotation = FRotationMatrix::MakeFromX(ToPlayer).Rotator();
        LookAtRotation.Pitch = 0.0f;
        LookAtRotation.Roll = 0.0f;

        SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookAtRotation, DeltaTime, 5.0f));
    }

    if (!PlayerActor) return;

    float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());

    switch (CombatState)
    {
    case EBossCombatState::Idle:
        EnterState(EBossCombatState::Approaching);
        break;

    case EBossCombatState::Approaching:
    {
        if (Distance > 1500.f)
        {
            AddMovementInput((PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal(), 1.0f);

            TimeSinceLastCombat += DeltaTime;
            if (TimeSinceLastCombat > 4.0f && FMath::FRand() < 0.2f)
            {
                EnterState(EBossCombatState::Repositioning);
                TimeSinceLastCombat = 0.f;
                return;
            }
        }
        else if (Distance < 300.f)
        {
            Retreat();
            TimeSinceLastCombat = 0.f;
        }
        else
        {
            EnterState(EBossCombatState::CombatReady);
            TimeSinceLastCombat = 0.f;
        }
        break;
    }

    case EBossCombatState::RotatingToAttack:
    {
        if (!PlayerActor || !GetMesh()) break;

        FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
        FRotator TargetRot = FRotationMatrix::MakeFromX(ToPlayer).Rotator();
        TargetRot.Pitch = 0.f;
        TargetRot.Roll = 0.f;

        FRotator CurrentRot = GetActorRotation();
        FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, 12.0f);
        SetActorRotation(NewRot);

        float DeltaYaw = FRotator::NormalizeAxis(NewRot.Yaw - TargetRot.Yaw);
        if (FMath::Abs(DeltaYaw) < 1.0f)
        {
            StartDashSequence();                 //  ȸ�� �Ϸ� �� ���� ȣ��
            EnterState(EBossCombatState::Attacking);
        }

        break;
    }

    case EBossCombatState::CombatReady:
        EvaluateCombatReady(); // ���� ���� �˻� �� ����
        break;

    case EBossCombatState::Attacking:
    {
        if (!GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
        {
            EnterState(EBossCombatState::Cooldown);
        }
        else
        {
            // ���� ���̶�� ���� ����
            if (CurrentPattern == EAttackPattern::Dash)
            {
                FVector Direction = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                AddMovementInput(Direction, 1.0f);
            }
        }
        break;
    }


    case EBossCombatState::Cooldown:
        // �� ���¿����� �ڵ� ���� �� �� �� Ÿ�̸ӷ� ����
        break;

    case EBossCombatState::Repositioning:
    {
        if (!bCanAttack || GetWorldTimerManager().IsTimerActive(WaitTimer)) break;

        int32 Behavior = FMath::RandRange(0, 2); // 0 = ����, 1 = ȸ��, 2 = ���� �̵�

        switch (Behavior)
        {
        case 0: // FakeIdle: 0.8�� ����
            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::ResetAttack, 0.8f, false);
            break;

        case 1: // Retreat: ȸ�� + �ļ� �ൿ
            Retreat(); // ���ο��� LaunchCharacter()
            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::EnterCombatReady, 1.2f, false); // ���� �� �ٷ� ���� ���·� ����
            break;

        case 2: // CircleAroundPlayer: ���� �̵� + ����
            CircleAroundPlayer();
            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::EnterCombatReady, 1.2f, false);
            break;
        }

        break;
    }


    case EBossCombatState::Staggered:
        // �ǰ� ���׼� or ���� �� ó��
        break;
    }
}

void ATigerBossCharacter::StartFallSequence()
{
    // EntryMontage�� �������� Ȯ�� �� ���� ����
    if (GetMesh() && GetMesh()->GetAnimInstance() &&
        GetMesh()->GetAnimInstance()->Montage_IsPlaying(EntryMontage))
    {
        // ���� ��� ���̸� ���� �ִٰ� �ٽ� Ȯ��
        GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::StartFallSequence, 0.1f, false);
        return;
    }

    // �ִϸ��̼��� �����ٸ� ���� FSM ����
    EnterCombatReady();
}



void ATigerBossCharacter::EnterCombatReady()
{
    bCanAttack = true;
    EnterState(EBossCombatState::CombatReady);
    EvaluateCombatReady(); // ù ���� �ٷ� ��
}



void ATigerBossCharacter::EnterState(EBossCombatState NewState)
{
    CombatState = NewState;

    switch (NewState)
    {
    case EBossCombatState::Cooldown:
        GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::ResetAttack, 1.0f, false);
        break;

    case EBossCombatState::Idle:
        bCanAttack = true;
        break;

    default:
        break;
    }
}




void ATigerBossCharacter::DecideAndExecuteAttack()
{
    if (!PlayerActor) return;

    float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());
    TArray<EAttackPattern> PossiblePatterns;

    if (CurrentPhase == EBossPhase::Phase1)
    {
        if (Distance < 300.f)
        {
            PossiblePatterns = { EAttackPattern::Claw, EAttackPattern::Dash };
        }
        else if (Distance < 700.f)
        {
            PossiblePatterns = { EAttackPattern::Jump, EAttackPattern::WideClaw };
        }
        else
        {
            PossiblePatterns = { EAttackPattern::Dash };
        }
    }
    if (CurrentPhase == EBossPhase::Phase2 && FMath::FRand() < 0.3f)
    {
        PerformCloneSpawn();
        EnterState(EBossCombatState::Cooldown);
        return;
    }
    else // Phase 2
    {
        PossiblePatterns = {
            EAttackPattern::CloneSpawn,
            EAttackPattern::TeleportAndDash,
            EAttackPattern::MultiDash
        };

        if (FMath::FRand() < 0.3f)
        {
            PossiblePatterns.Add(EAttackPattern::Roar);
        }
    }

    int32 Index = FMath::RandRange(0, PossiblePatterns.Num() - 1);
    CurrentPattern = PossiblePatterns[Index];

    if (!bIsInCombo && FMath::FRand() < 0.4f)
    {
        StartComboPattern();
    }
    else
    {
        SelectAndExecuteAttack();
    }

}

// ���ุ ���
void ATigerBossCharacter::SelectAndExecuteAttack()
{
    switch (CurrentPattern)
    {
    case EAttackPattern::Claw: PerformClawAttack(); break;
    case EAttackPattern::Jump: PerformJumpAttack(); break;
    case EAttackPattern::Dash: PerformDashAttack(); break;
    case EAttackPattern::TeleportAndDash: PerformTeleportAndDash(); break;
    case EAttackPattern::CloneSpawn: PerformCloneSpawn(); break;
    case EAttackPattern::Roar: PerformRoar(); break;
    case EAttackPattern::MultiDash: PerformMultiDash(); break;
    case EAttackPattern::WideClaw: PerformWideClaw(); break;
    default: break;
    }

    bCanAttack = false;
    EnterState(EBossCombatState::Attacking);
}


void ATigerBossCharacter::ResetAttack()
{
    bCanAttack = true;
    EnterState(EBossCombatState::Idle);
}


void ATigerBossCharacter::SlowApproach()
{
    FVector Direction = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    AddMovementInput(Direction, 100.0f);
}

void ATigerBossCharacter::Retreat()
{
    if (!PlayerActor) return;

    // ���� RetreatMontage �� EvadeMontage + ���� ����
    if (EvadeMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        AnimInstance->Montage_Play(EvadeMontage);
        AnimInstance->Montage_JumpToSection(FName("evade"), EvadeMontage);
    }

    // 0.3�� �� ���� �̵� (�ִϸ��̼ǰ� Ÿ�̹� ���߱�)
    FTimerHandle RetreatMoveTimer;
    GetWorldTimerManager().SetTimer(RetreatMoveTimer, [this]()
        {
            int32 DirectionType = FMath::RandRange(0, 2); // 0:��, 1:��, 2:��
            FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
            FVector RetreatDir;

            switch (DirectionType)
            {
            case 0: RetreatDir = -ToPlayer.GetSafeNormal(); break;
            case 1: RetreatDir = FVector::CrossProduct(ToPlayer, FVector::UpVector).GetSafeNormal(); break;
            case 2: RetreatDir = -FVector::CrossProduct(ToPlayer, FVector::UpVector).GetSafeNormal(); break;
            }

            float RetreatSpeed = 1000.f;
            LaunchCharacter(RetreatDir * RetreatSpeed, true, true);

        }, 0.1f, false);

    // ���� �� ���� ����
    bCanAttack = false;
    GetWorldTimerManager().SetTimer(AttackCooldownTimer, this, &ATigerBossCharacter::ResetAttack, 0.8f, false);
}





void ATigerBossCharacter::FastCharge()
{
    FVector Direction = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    LaunchCharacter(Direction * 1200.0f, true, true);
}

void ATigerBossCharacter::CircleAroundPlayer()
{
    if (!PlayerActor) return;

    FVector ToPlayer = PlayerActor->GetActorLocation() - GetActorLocation();
    FVector Tangent = FVector::CrossProduct(ToPlayer, FVector::UpVector).GetSafeNormal();
    FVector ForwardOffset = ToPlayer.GetSafeNormal(); // �ణ ����

    FVector FinalDirection = (Tangent + ForwardOffset * 0.3f).GetSafeNormal();
    AddMovementInput(FinalDirection, 1.5f);
}


void ATigerBossCharacter::HandleClawDamageNotify()
{
    FVector Origin = GetActorLocation();
    float Radius = 150.0f;

    TArray<FOverlapResult> Overlaps;
    FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

    bool bHit = GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Origin + GetActorForwardVector() * 150.f,
        FQuat::Identity,
        ECC_Pawn,
        Shape
    );

    if (bHit)
    {
        for (auto& Result : Overlaps)
        {
            AActor* HitActor = Result.GetActor();
            if (HitActor && HitActor != this)
            {
                UGameplayStatics::ApplyDamage(HitActor, 10.0f, GetController(), this, nullptr);
            }
        }
    }
}

void ATigerBossCharacter::PerformClawAttack()
{
    if (!PlayerActor) return;

    FVector Dir = PlayerActor->GetActorLocation() - GetActorLocation();
    TargetRotationForAttack = FRotationMatrix::MakeFromX(Dir).Rotator();
    TargetRotationForAttack.Pitch = 0.f;
    TargetRotationForAttack.Roll = 0.f;

    bWantsToRotateToPlayer = true;

    PostRotationAction = [this]()
        {
            if (ClawMontage && GetMesh() && GetMesh()->GetAnimInstance())
            {
                GetMesh()->GetAnimInstance()->Montage_Play(ClawMontage);
                UGameplayStatics::PlaySoundAtLocation(this, Voice_Claw, GetActorLocation());
            }
        };
}


void ATigerBossCharacter::PerformJumpAttack()
{
    if (!PlayerActor) return;

    FVector Dir = PlayerActor->GetActorLocation() - GetActorLocation();
    TargetRotationForAttack = FRotationMatrix::MakeFromX(Dir).Rotator();
    TargetRotationForAttack.Pitch = 0.f;
    TargetRotationForAttack.Roll = 0.f;

    bWantsToRotateToPlayer = true;

    PostRotationAction = [this]()
        {
            if (JumpMontage && GetMesh() && GetMesh()->GetAnimInstance())
            {
                GetMesh()->GetAnimInstance()->Montage_Play(JumpMontage);
            }

            // 0.9�� �� ����
            // ���� ���� (0.9�� ��)
            FTimerHandle JumpDelayTimer;
            GetWorldTimerManager().SetTimer(JumpDelayTimer, [this]()
                {
                    FVector JumpTarget = PlayerActor->GetActorLocation();
                    FVector JumpDirection = (JumpTarget - GetActorLocation()).GetSafeNormal();
                    FVector LaunchVelocity = JumpDirection * 1200.f + FVector(0, 0, 600.f);
                    LaunchCharacter(LaunchVelocity, true, true);

                    if (Voice_Jump)
                    {
                        UGameplayStatics::PlaySoundAtLocation(this, Voice_Jump, GetActorLocation());
                    }

                    // ���� �� �浹 ���� �˹� ó�� (1�ʰ� ����)
                    TArray<AActor*> AlreadyHit;

                    // ���� �� �浹 ���� Ÿ�̸� ����
                    GetWorldTimerManager().SetTimer(AirHitTimer, FTimerDelegate::CreateLambda([this, AlreadyHit]() mutable
                        {
                            FVector Origin = GetActorLocation();
                            float Radius = 420.f;

                            TArray<FOverlapResult> Overlaps;
                            FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

                            if (GetWorld()->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, Shape))
                            {
                                for (auto& Result : Overlaps)
                                {
                                    if (IsValid(this))
                                    {
                                        CreateAttackTraceCPP();
                                    }
                                    AActor* HitActor = Result.GetActor();
                                    if (HitActor && HitActor != this && !AlreadyHit.Contains(HitActor))
                                    {
                                        AlreadyHit.Add(HitActor);

                                        if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                                        {
                                            FVector KnockbackDir = (HitChar->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                                            FVector KnockbackForce = KnockbackDir * 2000.f + FVector(0, 0, 300.f);
                                            HitChar->LaunchCharacter(KnockbackForce, true, true);

                                            
                                        }
                                    }
                                }
                            }

                        }), 0.1f, true);  // 0.1�� �������� �浹 ����


                    // �浹 ���� 1�� �� Ÿ�̸� ���� (���� ĸó �ʿ� ����!)
                    FTimerHandle ClearAirHitTimer;
                    GetWorldTimerManager().SetTimer(ClearAirHitTimer, [this]()
                        {
                            GetWorldTimerManager().ClearTimer(AirHitTimer);
                        }, 1.0f, false);

                    // ���� ����� (1�� ��)
                    FTimerHandle LandTimer;
                    GetWorldTimerManager().SetTimer(LandTimer, [this]()
                        {
                            FVector ImpactLocation = GetActorLocation();
                            float Radius = 1000.0f;

                            TArray<FOverlapResult> Overlaps;
                            FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

                            if (GetWorld()->OverlapMultiByChannel(Overlaps, ImpactLocation, FQuat::Identity, ECC_Pawn, Shape))
                            {
                                for (auto& Result : Overlaps)
                                {
                                    AActor* HitActor = Result.GetActor();
                                    if (HitActor && HitActor != this)
                                    {
                                        if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                                        {
                                            // ���鿡 ���� ���� ����
                                            if (UCharacterMovementComponent* MoveComp = HitChar->GetCharacterMovement())
                                            {
                                                if (!MoveComp->IsFalling()) // ������ �ƴϸ�
                                                {
                                                    // ���߿� ����
                                                    FVector UpForce = FVector(0, 0, 1000.0f);
                                                    HitChar->LaunchCharacter(UpForce, true, true);

                                                }
                                            }
                                        }
                                    }
                                }
                            }

                        }, 1.0f, false);  // ���� 1�� �� ����� �߻�


                }, 0.9f, false); // ���� Ÿ�̹�
        };
        StopAttackTraceCPP();
}


void ATigerBossCharacter::PerformDashAttack()
{
    FVector Dir = PlayerActor->GetActorLocation() - GetActorLocation();
    TargetRotationForAttack = FRotationMatrix::MakeFromX(Dir).Rotator();
    TargetRotationForAttack.Pitch = 0.f;
    TargetRotationForAttack.Roll = 0.f;

    bWantsToRotateToPlayer = true;

    PostRotationAction = [this]()
        {
            StartDashSequence(); // ȸ�� �Ϸ� �� ����
        };
}


void ATigerBossCharacter::StartDashSequence()
{
    if (!PlayerActor) return;

    AActor* PlayerRef = PlayerActor;
    TWeakObjectPtr<ATigerBossCharacter> WeakThis(this);

    if (DashMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->Montage_Play(DashMontage);
        if (Voice_Dash)
        {
            UGameplayStatics::PlaySoundAtLocation(this, Voice_Dash, GetActorLocation());
        }
    }

    // ���� Ÿ�̸� ����
    GetWorldTimerManager().ClearTimer(DashStartTimer);
    GetWorldTimerManager().ClearTimer(MovementTimer);
    GetWorldTimerManager().ClearTimer(CollisionTimer);

    // ���� ���� Ÿ�̸� (1.9�� ��)
    GetWorldTimerManager().SetTimer(DashStartTimer, FTimerDelegate::CreateLambda([WeakThis, PlayerRef]()
        {
            if (!WeakThis.IsValid()) return;

            FVector Forward = WeakThis->GetActorForwardVector();


            //  ���� ����
            if (UCharacterMovementComponent* MoveComp = WeakThis->GetCharacterMovement())
            {
                MoveComp->BrakingFrictionFactor = 0.f;
                MoveComp->BrakingDecelerationWalking = 0.f;
            }

            //  �浹�� Overlap���� ���� (�÷��̾�� �� ������)
            if (UCapsuleComponent* Capsule = WeakThis->GetCapsuleComponent())
            {
                Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
            }

            //  ����
            WeakThis->LaunchCharacter(Forward * 1200.0f, true, true);


            //  ���� + �浹 ���� Ÿ�̸� (1�� ��)
            FTimerHandle RestoreTimer;
            WeakThis->GetWorldTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateLambda([WeakThis]()
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
                    //  ���� �������� �浹 ������ ����
                    WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CollisionTimer);

                }), 1.0f, false);

            //  ���� ����
            WeakThis->GetWorldTimerManager().SetTimer(WeakThis->MovementTimer, FTimerDelegate::CreateLambda([WeakThis]()
                {
                    if (!WeakThis.IsValid()) return;

                    if (WeakThis->GetMesh() && WeakThis->GetMesh()->GetAnimInstance() &&
                        WeakThis->GetMesh()->GetAnimInstance()->Montage_IsPlaying(WeakThis->DashMontage))
                    {
                        FVector ForwardDir = WeakThis->GetActorForwardVector();
                        WeakThis->AddMovementInput(ForwardDir, 1.0f);
                    }

                }), 0.02f, true);

            //  �浹 ���� �� �˹�
            TWeakObjectPtr<AActor> WeakPlayer(PlayerRef);
            WeakThis->GetWorldTimerManager().SetTimer(WeakThis->CollisionTimer, FTimerDelegate::CreateLambda([WeakThis, WeakPlayer]()
                {
                    if (!WeakThis.IsValid() || !WeakPlayer.IsValid()) return;

                    FVector Origin = WeakThis->GetActorLocation();
                    float Radius = 420.0f;

                    TArray<FOverlapResult> Overlaps;
                    FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

                    if (WeakThis->GetWorld()->OverlapMultiByChannel(
                        Overlaps,
                        Origin + WeakThis->GetActorForwardVector() * 100.f,
                        FQuat::Identity,
                        ECC_Pawn,
                        Shape))
                    {
                        // �˹� �� ���� ���� �߻�
                        if (WeakThis.IsValid())
                        {
                            WeakThis->CreateAttackTraceCPP();  // �������Ʈ �Լ� ȣ��
                        }
                        for (auto& Result : Overlaps)
                        {
                            AActor* HitActor = Result.GetActor();
                            if (HitActor == WeakPlayer.Get())
                            {
                                FVector KnockbackDir = (HitActor->GetActorLocation() - WeakThis->GetActorLocation()).GetSafeNormal();
                                FVector KnockbackForce = KnockbackDir * 2000.0f + FVector(0, 0, 300.0f);

                                if (ACharacter* HitChar = Cast<ACharacter>(HitActor))
                                {
                                    HitChar->LaunchCharacter(KnockbackForce, true, true);
                                    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("player knockback by dash"));

                                    
                                }

                                // �˹� �� �浹 ���� ����
                                if (WeakThis->CollisionTimer.IsValid())
                                {
                                    WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CollisionTimer);
                                }
                            }
                        }
                    }

                }), 0.05f, true, 0.0f); // �˹��� ���� ���� 1�� �ĺ��� ����

        }), 1.9f, false);
        StopAttackTraceCPP();
}





void ATigerBossCharacter::PerformTeleportAndDash()
{
    if (ActiveClones.Num() == 0 || !PlayerActor) return;

    // 50% Ȯ��
    if (FMath::FRand() > 1.0f)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("TP Skip"));
        return;
    }

    int32 Index = FMath::RandRange(0, ActiveClones.Num() - 1);
    ABossCloneCharacter* TargetClone = ActiveClones[Index].Get();

    if (!IsValid(TargetClone)) return;

    // ��ġ ��ȯ
    FVector BossOldLocation = GetActorLocation();
    FVector CloneLocation = TargetClone->GetActorLocation();

    // ���� �� �н� ��ġ��
    SetActorLocation(CloneLocation);

    // �н� �� ���� ���� ��ġ��
    TargetClone->SetActorLocation(BossOldLocation);

    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("swap with clone"));

    // ���� �� �ٷ� ����
    PerformDashAttack();
}


void ATigerBossCharacter::PerformCloneSpawn()
{
    if (!PlayerActor || !CloneClass) return;

    ActiveClones.Empty();  // ���� ��ȯ�� Ŭ�� ��� ����

    FVector PlayerLocation = PlayerActor->GetActorLocation();

    for (int32 i = 0; i < NumClones; ++i)
    {
        float Angle = FMath::DegreesToRadians(360.f / NumClones * i);
        float RandomOffset = FMath::FRandRange(-100.f, 100.f);
        FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (CloneRadius + RandomOffset);
        FVector SpawnLocation = PlayerLocation + Offset;

        // ���� ����
        FHitResult Hit;
        FVector TraceStart = SpawnLocation + FVector(0, 0, 500.f);
        FVector TraceEnd = SpawnLocation - FVector(0, 0, 1000.f);
        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility))
        {
            SpawnLocation.Z = Hit.ImpactPoint.Z + 5.f;
        }
        else
        {
            SpawnLocation.Z += 150.f;
        }

        FActorSpawnParameters Params;
        ABossCloneCharacter* Clone = GetWorld()->SpawnActor<ABossCloneCharacter>(CloneClass, SpawnLocation, FRotator::ZeroRotator, Params);

        if (!Clone)
        {
            //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("can't spawn"));
            continue;
        }

        //  Ŭ�� �̵� ���� + �迭�� ����
        if (UCharacterMovementComponent* MoveComp = Clone->GetCharacterMovement())
        {
            MoveComp->SetMovementMode(MOVE_Walking);
            MoveComp->GravityScale = 1.0f;
        }

        ActiveClones.Add(Clone);  // ������ ����

        // �ı� Ÿ�̸� (�迭������ ����)
        FTimerHandle DestroyHandle;
        GetWorldTimerManager().SetTimer(DestroyHandle, FTimerDelegate::CreateLambda([this, Clone]()
            {
                if (IsValid(Clone))
                {
                    ActiveClones.Remove(Clone);
                    Clone->Destroy();
                }
            }), CloneLifetime, false);
    }

    UE_LOG(LogTemp, Warning, TEXT("CloneSpawn: %d clones around player"), NumClones);

    if (FMath::FRand() < 1.0f)
    {
        PerformTeleportAndDash();
    }
}



void ATigerBossCharacter::PerformRoar() {}
void ATigerBossCharacter::PerformMultiDash() {}
void ATigerBossCharacter::PerformWideClaw() {}

void ATigerBossCharacter::ExecuteComboStep()
{
    if (!PlayerActor) return;

    switch (ComboStep)
    {
    case 0:
        PerformClawAttack();
        break;
    case 1:
        PerformDashAttack();
        break;
    case 2:
        PerformJumpAttack();
        break;
    default:
        bIsInCombo = false;
        ComboStep = 0;
        return;
    }

    ComboStep++;
    GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::ExecuteComboStep, 1.2f, false);
}


void ATigerBossCharacter::StartComboPattern()
{
    bIsInCombo = true;
    ComboStep = 0;
    ExecuteComboStep();
}

void ATigerBossCharacter::EvaluateCombatReady()
{
    if (!bCanAttack || !GetMesh() || !GetMesh()->GetAnimInstance() || GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr)) return;

    float Distance = FVector::Dist(GetActorLocation(), PlayerActor->GetActorLocation());

    // Phase 2���� 30% Ȯ���� �н� ��ȯ
    if (CurrentPhase == EBossPhase::Phase2 && FMath::FRand() < 0.3f)
    {
        PerformCloneSpawn(); // �� �ȿ��� 50% Ȯ���� �ڷ���Ʈ + �뽬�� �߻���
        bCanAttack = false;
        EnterState(EBossCombatState::Cooldown);
        return;
    }

    // �Ÿ� ��� ���� ����
    if (Distance < 800.f)
        PerformClawAttack();
    else if (Distance < 1300.f)
        PerformDashAttack();
    else if (Distance < 2000.f)
        PerformJumpAttack();
    else
        EnterState(EBossCombatState::Repositioning);

    bCanAttack = false;
    EnterState(EBossCombatState::Attacking);
}

void ATigerBossCharacter::HandleHealthChanged(float CurrentHP, float MaxHP)
{
    CachedCurrentHp = CurrentHP;
    CachedMaxHp = MaxHP;

    UE_LOG(LogTemp, Warning, TEXT("���� ü�� ����: %.1f / %.1f"), CurrentHP, MaxHP);

    CheckPhaseTransition();
}

void ATigerBossCharacter::callDelegate(float CurrentHP, float MaxHP)
{
    OnHealthChanged.AddDynamic(this, &ATigerBossCharacter::TestPrint_Two);
    if (OnHealthChanged.IsBound())
    {
        OnHealthChanged.Broadcast(CurrentHP, MaxHP);
    }
}

void ATigerBossCharacter::TestPrint_Two(float CurrentHP, float MaxHP)
{
    PercentHP = (MaxHP > 0.f) ? (CurrentHP / MaxHP) : 0.f;
    CurrentHealth = CurrentHP;
}

void ATigerBossCharacter::Dead()
{
    //if (bIsDead) return;  // �ߺ� ����

    bIsDead = true;

    // FSM �� �̵� ����
    GetCharacterMovement()->DisableMovement();
    SetActorTickEnabled(false);

    // �浹 ��Ȱ��ȭ (�ʿ� ��)
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // ��� Ÿ�̸� ����
    GetWorldTimerManager().ClearAllTimersForObject(this);

    // �ִϸ��̼� �ߴ� �� ��� �ִ� ���
    if (DeathMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* Anim = GetMesh()->GetAnimInstance();
        Anim->StopAllMontages(0.2f);
        Anim->Montage_Play(DeathMontage);
    }

    // ��� �ִ� ���� �� �����ϰ� �ʹٸ� �Ʒ� Ÿ�̸� ���
    //GetWorldTimerManager().SetTimer(DeathCleanupHandle, this, &ATigerBossCharacter::OnDeathCleanup, 5.f, false);
}
