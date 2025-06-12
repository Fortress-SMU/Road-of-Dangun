#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DragonRandomFlyComponent.generated.h"

// 헤딩 모드 진입 시 발생하는 이벤트 디스패처
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeadingEventDispatcher);

// 헤딩 모드 종료 시 발생하는 이벤트 디스패처
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

	// 헤딩 모드를 강제로 종료하는 함수
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ForceEndHeadingMode();

	// 헤딩 모드 시작 시 호출되는 이벤트 (블루프린트에서 바인딩 가능)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHeadingEventDispatcher OnHeadingModeTriggered;

	// 헤딩 모드 종료 시 호출되는 이벤트 (블루프린트에서 바인딩 가능)
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHeadingEndEventDispatcher OnHeadingModeEnded;

	// 초기 비행 소리
	UPROPERTY(EditAnywhere, Category = "Sound")
	USoundBase* IntroFlySound;

	// 랜덤 비행 소리
	UPROPERTY(EditAnywhere, Category = "Sound")
	USoundBase* RandomFlySound;

	// 초기 이동 경로 설정 (레벨에서 수동 지정)
	UPROPERTY(EditAnywhere, Category = "Intro Movement")
	TArray<AActor*> IntroWaypoints;

	int32 IntroIndex = 0;
	bool bDoingIntroMove = true;

private:
	// =============================
	// 드래곤의 비행 경로 설정
	// =============================

	// 드래곤이 비행하는 포인트 리스트 (레벨에서 수동 지정)
	UPROPERTY(EditInstanceOnly, Category = "Flying")
	TArray<AActor*> Waypoints;

	// 이전 프레임에서의 드래곤 위치
	FVector PreviousLocation = FVector::ZeroVector;

	// 정지 상태 지속 시간 (고착 상태 탐지용)
	float StuckTimer = 0.0f;

	// 이동 속도
	UPROPERTY()
	float Speed = 500.0f;

	// 목표 지점 도달 판정 거리
	UPROPERTY()
	float ArrivalDistance = 300.0f;

	// 플레이어 추적 시 도달 판정 거리
	UPROPERTY()
	float TrackingDistanceThreshold = 50.0f;

	// 파이어볼 공격 시 목표 도달 거리
	UPROPERTY()
	float FireballAttackDistance = 1500.0f;

	// 파이어볼이 발사될 위치 & 헤딩 판정 기준점
	UPROPERTY()
	USceneComponent* FireballSpawnPoint;

	// 파이어볼 발사 속도 (내부 전용, 코드에서만 사용됨)
	static constexpr float FireballSpeed = 1800.0f;

	// =============================
	// 공격 관련 설정
	// =============================

	// 발사할 투사체 클래스 (블루프린트에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> ProjectileClass;

	// =============================
	// 내부 참조 변수
	// =============================

	// 드래곤 본인 액터
	AActor* Owner = nullptr;

	// 플레이어 캐릭터
	AActor* Player = nullptr;

	// 현재 목표 위치
	FVector CurrentTarget = FVector::ZeroVector;

	// =============================
	// 보간 관련 변수
	// =============================

	// 이전 방향 (회전 보간 시 사용 가능)
	FVector PreviousDirection = FVector::ForwardVector;

	// 이전 본 위치 (부드러운 이동을 위한 보간 기준점)
	FVector LastBoneLocation = FVector::ZeroVector;

	// =============================
	// 행동 상태 플래그
	// =============================

	// 플레이어 추적 모드 여부
	bool bTrackLivePlayer = false;

	// 파이어볼 모드 여부
	bool bFireballMode = false;

	// 파이어볼 한 번만 발사되었는지 여부
	bool bFiredOnce = false;

	// 다음 행동을 무조건 랜덤 비행으로 설정할지 여부
	bool bForceRandomFlyNext = false;

	// 드래곤이 사망했는지 여부
	bool bIsDead = false;

	// =============================
	// 헤딩 모드 제한 시간
	// =============================

	// 헤딩 모드 종료용 타이머 핸들
	FTimerHandle HeadingTimeoutHandle;

	// 헤딩 모드 유지 시간 (초)
	float HeadingTimeoutDuration = 6.0f;

	// =============================
	// 내부 동작 함수
	// =============================

	// 목표 위치로 이동 처리
	void MoveToTarget(float DeltaTime);

	// 다음 행동을 결정해서 새로운 목표 위치 설정
	void PickNewTarget();

	// 랜덤 위치로 이동하는 로직
	void GoToRandomFly(const FVector& CurrentLocation);

	// 저장된 목표 위치로 투사체 발사
	void FireProjectileToStoredTarget();

	// 디버그 메시지 출력
	void ShowActionMessage(int32 ActionIndex, const FString& ActionName);
};
