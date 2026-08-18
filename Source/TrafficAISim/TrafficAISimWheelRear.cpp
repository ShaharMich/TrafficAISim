// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrafficAISimWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UTrafficAISimWheelRear::UTrafficAISimWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}