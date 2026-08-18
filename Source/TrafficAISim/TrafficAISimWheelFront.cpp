// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrafficAISimWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UTrafficAISimWheelFront::UTrafficAISimWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}