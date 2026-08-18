// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TrafficAISimPawn.h"
#include "TrafficAISimSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class ATrafficAISimSportsCar : public ATrafficAISimPawn
{
	GENERATED_BODY()
	
public:

	ATrafficAISimSportsCar();
};
