// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadOfDangunGameMode.h"
#include "RoadOfDangunCharacter.h"
#include "UObject/ConstructorHelpers.h"

ARoadOfDangunGameMode::ARoadOfDangunGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
