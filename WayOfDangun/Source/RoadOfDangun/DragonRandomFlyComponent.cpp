#include "DragonRandomFlyComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/ProjectileMovementComponent.h"

UDragonRandomFlyComponent::UDragonRandomFlyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDragonRandomFlyComponent::BeginPlay()
{
	Super::BeginPlay();

	Owner = GetOwner();
	Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0); // 플레이어 포인터

	// 파이어볼 스폰 위치 검색
	if (Owner)
	{
		TArray<USceneComponent*> Children;
		Owner->GetComponents<USceneComponent>(Children);

		for (USceneComponent* Comp : Children)
		{
			if (Comp && Comp->GetName() == TEXT("FireballSpawnPoint"))
			{
				FireballSpawnPoint = Comp;
				break;
			}
		}
	}

	if (IntroWaypoints.Num() > 0)
	{
		IntroIndex = 0;
		CurrentTarget = IntroWaypoints[IntroIndex]->GetActorLocation();
		bDoingIntroMove = true;

		// 인트로 무빙 사운드 재생
		if (IntroFlySound && Owner)
		{
			USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
			if (Mesh)
			{
				UGameplayStatics::SpawnSoundAttached(IntroFlySound, Mesh);
			}
		}
	}
	else
	{
		bDoingIntroMove = false;
		PickNewTarget();
	}
}

void UDragonRandomFlyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsDead || !Owner || Waypoints.Num() == 0) return; // 사망 시 중단

	// 플레이어 추적일 경우 현재 위치 업데이트
	if (bTrackLivePlayer && Player)
	{
		FVector PlayerLocation = Player->GetActorLocation();
		PlayerLocation.Z += 30.0f;
		CurrentTarget = PlayerLocation;

		// DrawDebugSphere(GetWorld(), CurrentTarget, 30.0f, 12, FColor::Green, false, 0.1f); // 디버그 구 표시
	}

	MoveToTarget(DeltaTime); // 이동 처리

	// 정지 상태 감지
	const float MovementThreshold = 5.0f;
	const float StuckTimeLimit = 1.5f;

	FVector CurrentLocation = Owner->GetActorLocation();
	float MovedDistance = FVector::Dist(CurrentLocation, PreviousLocation);

	if (MovedDistance < MovementThreshold)
		StuckTimer += DeltaTime;
	else
		StuckTimer = 0.0f;

	PreviousLocation = CurrentLocation;

	if (StuckTimer > StuckTimeLimit)
	{
		//ShowActionMessage(88, TEXT("Stuck Detected - Repicking Target"));
		PickNewTarget();
		StuckTimer = 0.0f;
	}
}

FVector LastBoneLocation = FVector::ZeroVector; // 이전 프레임의 본 위치

// 드래곤 이동 함수
void UDragonRandomFlyComponent::MoveToTarget(float DeltaTime)
{
	if (bIsDead || !Owner || !FireballSpawnPoint) return;

	// 본 위치 보간
	FVector RawLocation = FireballSpawnPoint->GetComponentLocation();
	FVector SmoothLocation = FMath::VInterpTo(LastBoneLocation, RawLocation, DeltaTime, 8.0f);
	LastBoneLocation = SmoothLocation;

	// 이동 방향 및 거리 계산
	FVector Direction = (CurrentTarget - SmoothLocation).GetSafeNormal();
	float RemainingDistance = FVector::Dist(SmoothLocation, CurrentTarget);
	float ArrivalDist = bTrackLivePlayer ? (bFireballMode ? FireballAttackDistance : TrackingDistanceThreshold) : ArrivalDistance;

	// 방향 회전 처리
	FRotator CurrentRotation = Owner->GetActorRotation();
	FRotator TargetRotation = Direction.Rotation();
	float YawDiff = FMath::Abs(FRotator::NormalizeAxis(TargetRotation.Yaw - CurrentRotation.Yaw));

	// 회전 각도 제한
	if (YawDiff > 175.0f)
	{
		//ShowActionMessage(99, TEXT("Too Far Behind - Repick"));
		PickNewTarget();
		return;
	}

	// 이동 처리
	FVector MoveDelta = Direction * Speed * DeltaTime;
	FVector NewActorLocation = (MoveDelta.Size() > RemainingDistance)
		? Owner->GetActorLocation() + Direction * RemainingDistance
		: Owner->GetActorLocation() + MoveDelta;

	// 충돌 체크
	FHitResult Hit;
	Owner->SetActorLocation(NewActorLocation, true, &Hit);

	if (Hit.bBlockingHit && Hit.PenetrationDepth > 10.0f)
	{
		//ShowActionMessage(77, FString::Printf(TEXT("Move Blocked! %.2f"), Hit.PenetrationDepth));
		PickNewTarget();
		return;
	}

	// 회전 처리
	Owner->SetActorRotation(FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 0.6f));

	// 도착 처리
	if (RemainingDistance < ArrivalDist)
	{
		if (bDoingIntroMove)
		{
			IntroIndex++;
			if (IntroIndex < IntroWaypoints.Num())
			{
				Speed = 1800.0f; // Intro 이동 속도
				CurrentTarget = IntroWaypoints[IntroIndex]->GetActorLocation();
				//ShowActionMessage(10, FString::Printf(TEXT("Intro Move %d"), IntroIndex));
			}
			else
			{
				Speed = 500.0f;
				bDoingIntroMove = false;
				PickNewTarget(); // Intro 끝나면 일반 행동 시작
			}
			return;
		}

		// 일반 모드일 때 파이어볼 처리
		if (bTrackLivePlayer && bFireballMode && !bFiredOnce)
		{
			FireProjectileToStoredTarget();
			bFiredOnce = true;
		}

		PickNewTarget();
	}
}

void UDragonRandomFlyComponent::PickNewTarget()
{
	if (bIsDead || !Owner || Waypoints.Num() == 0) return;

	// 현재 상태 초기화
	bool bWasInHeadingMode = bTrackLivePlayer && !bFireballMode;
	bTrackLivePlayer = false;
	bFireballMode = false;
	bFiredOnce = false;

	// 이전 모드 종료 처리
	if (bWasInHeadingMode)
	{
		OnHeadingModeEnded.Broadcast();
		GetWorld()->GetTimerManager().ClearTimer(HeadingTimeoutHandle);
	}

	// 드래곤 머리 위치 참조
	USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return;
	FVector CurrentLocation = Mesh->GetBoneLocation(FName("D_Head"));

	// 강제 랜덤 이동 설정 시
	if (bForceRandomFlyNext)
	{
		bForceRandomFlyNext = false;
		GoToRandomFly(CurrentLocation);
		return;
	}

	// 행동 결정
	int32 ActionRoll = FMath::RandRange(0, 6);

	if (ActionRoll <= 3) // 0, 1, 2, 3 - 랜덤 이동
	{
		GoToRandomFly(CurrentLocation);
	}
	else if (ActionRoll <= 5) // 4, 5 - 플레이어 추적 & 헤딩 모드
	{
		bTrackLivePlayer = true;
		bFireballMode = false;
		bForceRandomFlyNext = true;
		CurrentTarget = FVector::ZeroVector;
		//ShowActionMessage(2, TEXT("Heading"));

		OnHeadingModeTriggered.Broadcast();
		GetWorld()->GetTimerManager().SetTimer(HeadingTimeoutHandle, this, &UDragonRandomFlyComponent::ForceEndHeadingMode, HeadingTimeoutDuration, false);
	}
	else if (ActionRoll == 6) // 6 - 플레이어 추적 & 파이어볼 모드
	{
		bTrackLivePlayer = true;
		bFireballMode = true;
		bForceRandomFlyNext = true;
		CurrentTarget = FVector::ZeroVector;
		//ShowActionMessage(3, TEXT("Fire Ball"));
	}
}

// 랜덤 이동 함수
void UDragonRandomFlyComponent::GoToRandomFly(const FVector& CurrentLocation)
{
	const int32 MaxTries = 10;
	const float MinDistance = 3000.0f;  // 다음 목표의 최소 거리 설정

	for (int32 Try = 0; Try < MaxTries; ++Try)
	{
		int32 Index = FMath::RandRange(0, Waypoints.Num() - 1);
		FVector Candidate = Waypoints[Index]->GetActorLocation();

		if (FVector::Dist(CurrentLocation, Candidate) >= MinDistance)
		{
			CurrentTarget = Candidate;
			//ShowActionMessage(1, TEXT("Random Fly"));

			if (RandomFlySound && Owner)
			{
				USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
				if (Mesh)
				{
					UGameplayStatics::SpawnSoundAttached(RandomFlySound, Mesh);
				}
			}
			return;
		}
	}

	// 실패 시 무작위 선택
	CurrentTarget = Waypoints[FMath::RandRange(0, Waypoints.Num() - 1)]->GetActorLocation();
	//ShowActionMessage(1, TEXT("Random Fly (Fallback)"));
}

// 파이어볼 발사 함수
void UDragonRandomFlyComponent::FireProjectileToStoredTarget()
{
	if (bIsDead || !ProjectileClass || !FireballSpawnPoint || !Player) return;

	// 발사 위치 및 타겟
	FVector Start = FireballSpawnPoint->GetComponentLocation();
	FVector Target = Player->GetActorLocation();
	Target.Z += 40.0f;

	// 디버그 시각화
	//DrawDebugSphere(GetWorld(), Start, 30.0f, 12, FColor::Blue, false, 2.0f);
	//DrawDebugSphere(GetWorld(), Target, 30.0f, 12, FColor::Red, false, 2.0f);
	//DrawDebugLine(GetWorld(), Start, Target, FColor::Cyan, false, 2.0f, 0, 2.0f);

	// 발사 방향 및 스폰
	FVector Direction = (Target - Start).GetSafeNormal();
	FRotator Rotation = Direction.Rotation();
	FTransform SpawnTM = FTransform(Rotation, Start);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Projectile = GetWorld()->SpawnActor<AActor>(ProjectileClass, SpawnTM, Params);
	if (!Projectile) return;

	UProjectileMovementComponent* MoveComp = Projectile->FindComponentByClass<UProjectileMovementComponent>();
	if (!MoveComp)
	{
		// 없으면 새로 생성
		MoveComp = NewObject<UProjectileMovementComponent>(Projectile);
		if (MoveComp)
		{
			MoveComp->RegisterComponent();
			MoveComp->UpdatedComponent = Projectile->GetRootComponent();
			MoveComp->InitialSpeed = FireballSpeed;
			MoveComp->MaxSpeed = FireballSpeed;
			MoveComp->bRotationFollowsVelocity = true;
			MoveComp->bShouldBounce = false;
			MoveComp->Velocity = Direction * FireballSpeed;
		}
	}
	else
	{
		MoveComp->Velocity = Direction * FireballSpeed;
	}
}

// 디버그 메시지 출력 함수
void UDragonRandomFlyComponent::ShowActionMessage(int32 ActionIndex, const FString& ActionName)
{
	if (GEngine)
	{
		//FString Message = FString::Printf(TEXT("Action (%d) : %s"), ActionIndex, *ActionName);
		//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Message);
	}
}

// 헤딩 모드 강제 종료 함수
void UDragonRandomFlyComponent::ForceEndHeadingMode()
{
	if (bIsDead) return;

	if (bTrackLivePlayer && !bFireballMode)
	{
		//ShowActionMessage(4, TEXT("Force End Heading"));
		PickNewTarget();
	}
}
