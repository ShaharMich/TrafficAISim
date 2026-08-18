// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrafficAISimGameMode.h"
#include "TrafficAISimPlayerController.h"

ATrafficAISimGameMode::ATrafficAISimGameMode()
{
	PlayerControllerClass = ATrafficAISimPlayerController::StaticClass();
}
