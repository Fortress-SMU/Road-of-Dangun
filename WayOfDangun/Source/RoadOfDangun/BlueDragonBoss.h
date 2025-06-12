// BlueDragonBoss.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlueDragonBoss.generated.h"

class USkeletalMeshComponent;
class UDragonRandomFlyComponent;

UCLASS(Blueprintable)
class RoadOfDangun_API ABlueDragonBoss : public AActor
{
	GENERATED_BODY()

public:
	ABlueDragonBoss();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* DragonMesh;

	// Blueprint에서 호출할 사망 처리 함수
	UFUNCTION(BlueprintCallable, Category = "Death")
	void HandleDeath();
};
