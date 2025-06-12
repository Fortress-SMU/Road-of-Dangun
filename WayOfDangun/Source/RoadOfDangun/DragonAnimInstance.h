#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DragonAnimInstance.generated.h"

UCLASS(Blueprintable)
class RoadOfDangun_API UDragonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	// 필요 시 여기서 LookAt, 공격 상태 등 기능 추가 가능
};
