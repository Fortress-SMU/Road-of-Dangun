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

    // 플레이어 추적
    PlayerActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    CombatState = EBossCombatState::Idle;
    bCanAttack = false;
    bIsInCombo = false;
    ComboStep = 0;


    // 낙하 시작 위치 저장
    FallStartLocation = GetActorLocation();

    // 바닥 찾기 (라인 트레이스)
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

    // 중력 제거
    GetCharacterMovement()->SetMovementMode(MOVE_None);

    // 입장 애니메이션 재생
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

            // 위치 강제 고정
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

    //  낙하가 끝난 경우에만 FSM 실행
    CheckPhaseTransition();
    TickCombatFSM(DeltaTime);

    // 회전 처리
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
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Phase 2 진입!"));
        // PerformRoar(); 등 추가 가능
    }
}


void ATigerBossCharacter::TickCombatFSM(float DeltaTime)
{
    if (bIsDead) return;  // 죽었으면 FSM 비활성화
    if (!PlayerActor) return;
    if (!GetMesh() || !GetMesh()->GetAnimInstance()) return;
    // 공격 중일 땐 회전 금지
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
            StartDashSequence();                 //  회전 완료 후 직접 호출
            EnterState(EBossCombatState::Attacking);
        }

        break;
    }

    case EBossCombatState::CombatReady:
        EvaluateCombatReady(); // 공격 조건 검사 및 실행
        break;

    case EBossCombatState::Attacking:
    {
        if (!GetMesh()->GetAnimInstance()->Montage_IsPlaying(nullptr))
        {
            EnterState(EBossCombatState::Cooldown);
        }
        else
        {
            // 돌진 중이라면 전진 유지
            if (CurrentPattern == EAttackPattern::Dash)
            {
                FVector Direction = (PlayerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                AddMovementInput(Direction, 1.0f);
            }
        }
        break;
    }


    case EBossCombatState::Cooldown:
        // 이 상태에서는 자동 전이 안 함 → 타이머로 제어
        break;

    case EBossCombatState::Repositioning:
    {
        if (!bCanAttack || GetWorldTimerManager().IsTimerActive(WaitTimer)) break;

        int32 Behavior = FMath::RandRange(0, 2); // 0 = 정지, 1 = 회피, 2 = 원형 이동

        switch (Behavior)
        {
        case 0: // FakeIdle: 0.8초 정지
            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::ResetAttack, 0.8f, false);
            break;

        case 1: // Retreat: 회피 + 후속 행동
            Retreat(); // 내부에서 LaunchCharacter()
            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::EnterCombatReady, 1.2f, false); // 후퇴 후 바로 공격 상태로 전이
            break;

        case 2: // CircleAroundPlayer: 원형 이동 + 전이
            CircleAroundPlayer();
            GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::EnterCombatReady, 1.2f, false);
            break;
        }

        break;
    }


    case EBossCombatState::Staggered:
        // 피격 리액션 or 멈춤 등 처리
        break;
    }
}

void ATigerBossCharacter::StartFallSequence()
{
    // EntryMontage가 끝났는지 확인 후 전투 시작
    if (GetMesh() && GetMesh()->GetAnimInstance() &&
        GetMesh()->GetAnimInstance()->Montage_IsPlaying(EntryMontage))
    {
        // 아직 재생 중이면 조금 있다가 다시 확인
        GetWorldTimerManager().SetTimer(WaitTimer, this, &ATigerBossCharacter::StartFallSequence, 0.1f, false);
        return;
    }

    // 애니메이션이 끝났다면 전투 FSM 시작
    EnterCombatReady();
}



void ATigerBossCharacter::EnterCombatReady()
{
    bCanAttack = true;
    EnterState(EBossCombatState::CombatReady);
    EvaluateCombatReady(); // 첫 공격 바로 평가
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

// 실행만 담당
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

    // 기존 RetreatMontage → EvadeMontage + 섹션 지정
    if (EvadeMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
        AnimInstance->Montage_Play(EvadeMontage);
        AnimInstance->Montage_JumpToSection(FName("evade"), EvadeMontage);
    }

    // 0.3초 후 실제 이동 (애니메이션과 타이밍 맞추기)
    FTimerHandle RetreatMoveTimer;
    GetWorldTimerManager().SetTimer(RetreatMoveTimer, [this]()
        {
            int32 DirectionType = FMath::RandRange(0, 2); // 0:뒤, 1:좌, 2:우
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

    // 후퇴 중 공격 방지
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
    FVector ForwardOffset = ToPlayer.GetSafeNormal(); // 약간 전진

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

            // 0.9초 후 점프
            // 점프 실행 (0.9초 뒤)
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

                    // 점프 중 충돌 감지 넉백 처리 (1초간 감지)
                    TArray<AActor*> AlreadyHit;

                    // 점프 중 충돌 감지 타이머 설정
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

                        }), 0.1f, true);  // 0.1초 간격으로 충돌 감지


                    // 충돌 감지 1초 후 타이머 제거 (이제 캡처 필요 없음!)
                    FTimerHandle ClearAirHitTimer;
                    GetWorldTimerManager().SetTimer(ClearAirHitTimer, [this]()
                        {
                            GetWorldTimerManager().ClearTimer(AirHitTimer);
                        }, 1.0f, false);

                    // 착지 충격파 (1초 후)
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
                                            // 지면에 있을 때만 판정
                                            if (UCharacterMovementComponent* MoveComp = HitChar->GetCharacterMovement())
                                            {
                                                if (!MoveComp->IsFalling()) // 공중이 아니면
                                                {
                                                    // 공중에 띄우기
                                                    FVector UpForce = FVector(0, 0, 1000.0f);
                                                    HitChar->LaunchCharacter(UpForce, true, true);

                                                }
                                            }
                                        }
                                    }
                                }
                            }

                        }, 1.0f, false);  // 착지 1초 후 충격파 발생


                }, 0.9f, false); // 점프 타이밍
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
            StartDashSequence(); // 회전 완료 후 실행
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

    // 기존 타이머 제거
    GetWorldTimerManager().ClearTimer(DashStartTimer);
    GetWorldTimerManager().ClearTimer(MovementTimer);
    GetWorldTimerManager().ClearTimer(CollisionTimer);

    // 돌진 시작 타이머 (1.9초 뒤)
    GetWorldTimerManager().SetTimer(DashStartTimer, FTimerDelegate::CreateLambda([WeakThis, PlayerRef]()
        {
            if (!WeakThis.IsValid()) return;

            FVector Forward = WeakThis->GetActorForwardVector();


            //  마찰 제거
            if (UCharacterMovementComponent* MoveComp = WeakThis->GetCharacterMovement())
            {
                MoveComp->BrakingFrictionFactor = 0.f;
                MoveComp->BrakingDecelerationWalking = 0.f;
            }

            //  충돌을 Overlap으로 설정 (플레이어와 안 막히게)
            if (UCapsuleComponent* Capsule = WeakThis->GetCapsuleComponent())
            {
                Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
            }

            //  돌진
            WeakThis->LaunchCharacter(Forward * 1200.0f, true, true);


            //  마찰 + 충돌 복구 타이머 (1초 뒤)
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
                    //  돌진 끝났으니 충돌 감지도 종료
                    WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CollisionTimer);

                }), 1.0f, false);

            //  방향 유지
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

            //  충돌 감지 및 넉백
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
                        // 넉백 후 공격 판정 발생
                        if (WeakThis.IsValid())
                        {
                            WeakThis->CreateAttackTraceCPP();  // 블루프린트 함수 호출
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

                                // 넉백 후 충돌 감지 종료
                                if (WeakThis->CollisionTimer.IsValid())
                                {
                                    WeakThis->GetWorldTimerManager().ClearTimer(WeakThis->CollisionTimer);
                                }
                            }
                        }
                    }

                }), 0.05f, true, 0.0f); // 넉백은 돌진 시작 1초 후부터 감지

        }), 1.9f, false);
        StopAttackTraceCPP();
}





void ATigerBossCharacter::PerformTeleportAndDash()
{
    if (ActiveClones.Num() == 0 || !PlayerActor) return;

    // 50% 확률
    if (FMath::FRand() > 1.0f)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("TP Skip"));
        return;
    }

    int32 Index = FMath::RandRange(0, ActiveClones.Num() - 1);
    ABossCloneCharacter* TargetClone = ActiveClones[Index].Get();

    if (!IsValid(TargetClone)) return;

    // 위치 교환
    FVector BossOldLocation = GetActorLocation();
    FVector CloneLocation = TargetClone->GetActorLocation();

    // 보스 → 분신 위치로
    SetActorLocation(CloneLocation);

    // 분신 → 보스 원래 위치로
    TargetClone->SetActorLocation(BossOldLocation);

    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("swap with clone"));

    // 텔포 후 바로 돌진
    PerformDashAttack();
}


void ATigerBossCharacter::PerformCloneSpawn()
{
    if (!PlayerActor || !CloneClass) return;

    ActiveClones.Empty();  // 기존 소환된 클론 목록 비우기

    FVector PlayerLocation = PlayerActor->GetActorLocation();

    for (int32 i = 0; i < NumClones; ++i)
    {
        float Angle = FMath::DegreesToRadians(360.f / NumClones * i);
        float RandomOffset = FMath::FRandRange(-100.f, 100.f);
        FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (CloneRadius + RandomOffset);
        FVector SpawnLocation = PlayerLocation + Offset;

        // 지면 감지
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

        //  클론 이동 보장 + 배열에 저장
        if (UCharacterMovementComponent* MoveComp = Clone->GetCharacterMovement())
        {
            MoveComp->SetMovementMode(MOVE_Walking);
            MoveComp->GravityScale = 1.0f;
        }

        ActiveClones.Add(Clone);  // 추적용 저장

        // 파괴 타이머 (배열에서도 제거)
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

    // Phase 2에서 30% 확률로 분신 소환
    if (CurrentPhase == EBossPhase::Phase2 && FMath::FRand() < 0.3f)
    {
        PerformCloneSpawn(); // 이 안에서 50% 확률로 텔레포트 + 대쉬가 발생함
        bCanAttack = false;
        EnterState(EBossCombatState::Cooldown);
        return;
    }

    // 거리 기반 공격 선택
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

    UE_LOG(LogTemp, Warning, TEXT("보스 체력 갱신: %.1f / %.1f"), CurrentHP, MaxHP);

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
    //if (bIsDead) return;  // 중복 방지

    bIsDead = true;

    // FSM 및 이동 정지
    GetCharacterMovement()->DisableMovement();
    SetActorTickEnabled(false);

    // 충돌 비활성화 (필요 시)
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 모든 타이머 정지
    GetWorldTimerManager().ClearAllTimersForObject(this);

    // 애니메이션 중단 후 사망 애니 재생
    if (DeathMontage && GetMesh() && GetMesh()->GetAnimInstance())
    {
        UAnimInstance* Anim = GetMesh()->GetAnimInstance();
        Anim->StopAllMontages(0.2f);
        Anim->Montage_Play(DeathMontage);
    }

    // 사망 애니 끝난 뒤 제거하고 싶다면 아래 타이머 사용
    //GetWorldTimerManager().SetTimer(DeathCleanupHandle, this, &ATigerBossCharacter::OnDeathCleanup, 5.f, false);
}
