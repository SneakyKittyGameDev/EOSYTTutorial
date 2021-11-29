// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSYTTutorialGameMode.h"
#include "EOSYTTutorialCharacter.h"
#include "UObject/ConstructorHelpers.h"

AEOSYTTutorialGameMode::AEOSYTTutorialGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
