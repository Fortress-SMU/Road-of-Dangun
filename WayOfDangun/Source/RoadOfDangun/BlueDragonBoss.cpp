#include "BlueDragonBoss.h"
#include "Components/SkeletalMeshComponent.h"
#include "DragonRandomFlyComponent.h"
#include "Engine/World.h"

ABlueDragonBoss::ABlueDragonBoss()
{
	PrimaryActorTick.bCanEverTick = true;

	DragonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DragonMesh"));
	RootComponent = DragonMesh;

}

void ABlueDragonBoss::BeginPlay()
{
	Super::BeginPlay();
}

void ABlueDragonBoss::HandleDeath()
{

	// 물리 기반 시체 처리
	if (DragonMesh)
	{
		DragonMesh->SetSimulatePhysics(true);
		DragonMesh->SetEnableGravity(true);
	}

	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Blue Dragon Dead"));
}
