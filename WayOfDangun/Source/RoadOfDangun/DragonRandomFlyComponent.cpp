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
	Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0); // �÷��̾� ������

	// ���̾ ���� ��ġ �˻�
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

		// ��Ʈ�� ���� ���� ���
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

	if (bIsDead || !Owner || Waypoints.Num() == 0) return; // ��� �� �ߴ�

	// �÷��̾� ������ ��� ���� ��ġ ������Ʈ
	if (bTrackLivePlayer && Player)
	{
		FVector PlayerLocation = Player->GetActorLocation();
		PlayerLocation.Z += 30.0f;
		CurrentTarget = PlayerLocation;

		// DrawDebugSphere(GetWorld(), CurrentTarget, 30.0f, 12, FColor::Green, false, 0.1f); // ����� �� ǥ��
	}

	MoveToTarget(DeltaTime); // �̵� ó��

	// ���� ���� ����
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

FVector LastBoneLocation = FVector::ZeroVector; // ���� �������� �� ��ġ

// �巡�� �̵� �Լ�
void UDragonRandomFlyComponent::MoveToTarget(float DeltaTime)
{
	if (bIsDead || !Owner || !FireballSpawnPoint) return;

	// �� ��ġ ����
	FVector RawLocation = FireballSpawnPoint->GetComponentLocation();
	FVector SmoothLocation = FMath::VInterpTo(LastBoneLocation, RawLocation, DeltaTime, 8.0f);
	LastBoneLocation = SmoothLocation;

	// �̵� ���� �� �Ÿ� ���
	FVector Direction = (CurrentTarget - SmoothLocation).GetSafeNormal();
	float RemainingDistance = FVector::Dist(SmoothLocation, CurrentTarget);
	float ArrivalDist = bTrackLivePlayer ? (bFireballMode ? FireballAttackDistance : TrackingDistanceThreshold) : ArrivalDistance;

	// ���� ȸ�� ó��
	FRotator CurrentRotation = Owner->GetActorRotation();
	FRotator TargetRotation = Direction.Rotation();
	float YawDiff = FMath::Abs(FRotator::NormalizeAxis(TargetRotation.Yaw - CurrentRotation.Yaw));

	// ȸ�� ���� ����
	if (YawDiff > 175.0f)
	{
		//ShowActionMessage(99, TEXT("Too Far Behind - Repick"));
		PickNewTarget();
		return;
	}

	// �̵� ó��
	FVector MoveDelta = Direction * Speed * DeltaTime;
	FVector NewActorLocation = (MoveDelta.Size() > RemainingDistance)
		? Owner->GetActorLocation() + Direction * RemainingDistance
		: Owner->GetActorLocation() + MoveDelta;

	// �浹 üũ
	FHitResult Hit;
	Owner->SetActorLocation(NewActorLocation, true, &Hit);

	if (Hit.bBlockingHit && Hit.PenetrationDepth > 10.0f)
	{
		//ShowActionMessage(77, FString::Printf(TEXT("Move Blocked! %.2f"), Hit.PenetrationDepth));
		PickNewTarget();
		return;
	}

	// ȸ�� ó��
	Owner->SetActorRotation(FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 0.6f));

	// ���� ó��
	if (RemainingDistance < ArrivalDist)
	{
		if (bDoingIntroMove)
		{
			IntroIndex++;
			if (IntroIndex < IntroWaypoints.Num())
			{
				Speed = 1800.0f; // Intro �̵� �ӵ�
				CurrentTarget = IntroWaypoints[IntroIndex]->GetActorLocation();
				//ShowActionMessage(10, FString::Printf(TEXT("Intro Move %d"), IntroIndex));
			}
			else
			{
				Speed = 500.0f;
				bDoingIntroMove = false;
				PickNewTarget(); // Intro ������ �Ϲ� �ൿ ����
			}
			return;
		}

		// �Ϲ� ����� �� ���̾ ó��
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

	// ���� ���� �ʱ�ȭ
	bool bWasInHeadingMode = bTrackLivePlayer && !bFireballMode;
	bTrackLivePlayer = false;
	bFireballMode = false;
	bFiredOnce = false;

	// ���� ��� ���� ó��
	if (bWasInHeadingMode)
	{
		OnHeadingModeEnded.Broadcast();
		GetWorld()->GetTimerManager().ClearTimer(HeadingTimeoutHandle);
	}

	// �巡�� �Ӹ� ��ġ ����
	USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return;
	FVector CurrentLocation = Mesh->GetBoneLocation(FName("D_Head"));

	// ���� ���� �̵� ���� ��
	if (bForceRandomFlyNext)
	{
		bForceRandomFlyNext = false;
		GoToRandomFly(CurrentLocation);
		return;
	}

	// �ൿ ����
	int32 ActionRoll = FMath::RandRange(0, 6);

	if (ActionRoll <= 3) // 0, 1, 2, 3 - ���� �̵�
	{
		GoToRandomFly(CurrentLocation);
	}
	else if (ActionRoll <= 5) // 4, 5 - �÷��̾� ���� & ��� ���
	{
		bTrackLivePlayer = true;
		bFireballMode = false;
		bForceRandomFlyNext = true;
		CurrentTarget = FVector::ZeroVector;
		//ShowActionMessage(2, TEXT("Heading"));

		OnHeadingModeTriggered.Broadcast();
		GetWorld()->GetTimerManager().SetTimer(HeadingTimeoutHandle, this, &UDragonRandomFlyComponent::ForceEndHeadingMode, HeadingTimeoutDuration, false);
	}
	else if (ActionRoll == 6) // 6 - �÷��̾� ���� & ���̾ ���
	{
		bTrackLivePlayer = true;
		bFireballMode = true;
		bForceRandomFlyNext = true;
		CurrentTarget = FVector::ZeroVector;
		//ShowActionMessage(3, TEXT("Fire Ball"));
	}
}

// ���� �̵� �Լ�
void UDragonRandomFlyComponent::GoToRandomFly(const FVector& CurrentLocation)
{
	const int32 MaxTries = 10;
	const float MinDistance = 3000.0f;  // ���� ��ǥ�� �ּ� �Ÿ� ����

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

	// ���� �� ������ ����
	CurrentTarget = Waypoints[FMath::RandRange(0, Waypoints.Num() - 1)]->GetActorLocation();
	//ShowActionMessage(1, TEXT("Random Fly (Fallback)"));
}

// ���̾ �߻� �Լ�
void UDragonRandomFlyComponent::FireProjectileToStoredTarget()
{
	if (bIsDead || !ProjectileClass || !FireballSpawnPoint || !Player) return;

	// �߻� ��ġ �� Ÿ��
	FVector Start = FireballSpawnPoint->GetComponentLocation();
	FVector Target = Player->GetActorLocation();
	Target.Z += 40.0f;

	// ����� �ð�ȭ
	//DrawDebugSphere(GetWorld(), Start, 30.0f, 12, FColor::Blue, false, 2.0f);
	//DrawDebugSphere(GetWorld(), Target, 30.0f, 12, FColor::Red, false, 2.0f);
	//DrawDebugLine(GetWorld(), Start, Target, FColor::Cyan, false, 2.0f, 0, 2.0f);

	// �߻� ���� �� ����
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
		// ������ ���� ����
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

// ����� �޽��� ��� �Լ�
void UDragonRandomFlyComponent::ShowActionMessage(int32 ActionIndex, const FString& ActionName)
{
	if (GEngine)
	{
		//FString Message = FString::Printf(TEXT("Action (%d) : %s"), ActionIndex, *ActionName);
		//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, Message);
	}
}

// ��� ��� ���� ���� �Լ�
void UDragonRandomFlyComponent::ForceEndHeadingMode()
{
	if (bIsDead) return;

	if (bTrackLivePlayer && !bFireballMode)
	{
		//ShowActionMessage(4, TEXT("Force End Heading"));
		PickNewTarget();
	}
}
