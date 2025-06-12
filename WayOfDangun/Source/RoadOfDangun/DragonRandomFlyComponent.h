#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DragonRandomFlyComponent.generated.h"

// ��� ��� ���� �� �߻��ϴ� �̺�Ʈ ����ó
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeadingEventDispatcher);

// ��� ��� ���� �� �߻��ϴ� �̺�Ʈ ����ó
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeadingEndEventDispatcher);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RoadOfDangun_API UDragonRandomFlyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDragonRandomFlyComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ��� ��带 ������ �����ϴ� �Լ�
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ForceEndHeadingMode();

	// ��� ��� ���� �� ȣ��Ǵ� �̺�Ʈ (�������Ʈ���� ���ε� ����)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHeadingEventDispatcher OnHeadingModeTriggered;

	// ��� ��� ���� �� ȣ��Ǵ� �̺�Ʈ (�������Ʈ���� ���ε� ����)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHeadingEndEventDispatcher OnHeadingModeEnded;

	// �ʱ� ���� �Ҹ�
	UPROPERTY(EditAnywhere, Category = "Sound")
	USoundBase* IntroFlySound;

	// ���� ���� �Ҹ�
	UPROPERTY(EditAnywhere, Category = "Sound")
	USoundBase* RandomFlySound;

	// �ʱ� �̵� ��� ���� (�������� ���� ����)
	UPROPERTY(EditAnywhere, Category = "Intro Movement")
	TArray<AActor*> IntroWaypoints;

	int32 IntroIndex = 0;
	bool bDoingIntroMove = true;

private:
	// =============================
	// �巡���� ���� ��� ����
	// =============================

	// �巡���� �����ϴ� ����Ʈ ����Ʈ (�������� ���� ����)
	UPROPERTY(EditInstanceOnly, Category = "Flying")
	TArray<AActor*> Waypoints;

	// ���� �����ӿ����� �巡�� ��ġ
	FVector PreviousLocation = FVector::ZeroVector;

	// ���� ���� ���� �ð� (���� ���� Ž����)
	float StuckTimer = 0.0f;

	// �̵� �ӵ�
	UPROPERTY()
	float Speed = 500.0f;

	// ��ǥ ���� ���� ���� �Ÿ�
	UPROPERTY()
	float ArrivalDistance = 300.0f;

	// �÷��̾� ���� �� ���� ���� �Ÿ�
	UPROPERTY()
	float TrackingDistanceThreshold = 50.0f;

	// ���̾ ���� �� ��ǥ ���� �Ÿ�
	UPROPERTY()
	float FireballAttackDistance = 1500.0f;

	// ���̾�� �߻�� ��ġ & ��� ���� ������
	UPROPERTY()
	USceneComponent* FireballSpawnPoint;

	// ���̾ �߻� �ӵ� (���� ����, �ڵ忡���� ����)
	static constexpr float FireballSpeed = 1800.0f;

	// =============================
	// ���� ���� ����
	// =============================

	// �߻��� ����ü Ŭ���� (�������Ʈ���� ���� ����)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> ProjectileClass;

	// =============================
	// ���� ���� ����
	// =============================

	// �巡�� ���� ����
	AActor* Owner = nullptr;

	// �÷��̾� ĳ����
	AActor* Player = nullptr;

	// ���� ��ǥ ��ġ
	FVector CurrentTarget = FVector::ZeroVector;

	// =============================
	// ���� ���� ����
	// =============================

	// ���� ���� (ȸ�� ���� �� ��� ����)
	FVector PreviousDirection = FVector::ForwardVector;

	// ���� �� ��ġ (�ε巯�� �̵��� ���� ���� ������)
	FVector LastBoneLocation = FVector::ZeroVector;

	// =============================
	// �ൿ ���� �÷���
	// =============================

	// �÷��̾� ���� ��� ����
	bool bTrackLivePlayer = false;

	// ���̾ ��� ����
	bool bFireballMode = false;

	// ���̾ �� ���� �߻�Ǿ����� ����
	bool bFiredOnce = false;

	// ���� �ൿ�� ������ ���� �������� �������� ����
	bool bForceRandomFlyNext = false;

	// �巡���� ����ߴ��� ����
	bool bIsDead = false;

	// =============================
	// ��� ��� ���� �ð�
	// =============================

	// ��� ��� ����� Ÿ�̸� �ڵ�
	FTimerHandle HeadingTimeoutHandle;

	// ��� ��� ���� �ð� (��)
	float HeadingTimeoutDuration = 6.0f;

	// =============================
	// ���� ���� �Լ�
	// =============================

	// ��ǥ ��ġ�� �̵� ó��
	void MoveToTarget(float DeltaTime);

	// ���� �ൿ�� �����ؼ� ���ο� ��ǥ ��ġ ����
	void PickNewTarget();

	// ���� ��ġ�� �̵��ϴ� ����
	void GoToRandomFly(const FVector& CurrentLocation);

	// ����� ��ǥ ��ġ�� ����ü �߻�
	void FireProjectileToStoredTarget();

	// ����� �޽��� ���
	void ShowActionMessage(int32 ActionIndex, const FString& ActionName);
};
