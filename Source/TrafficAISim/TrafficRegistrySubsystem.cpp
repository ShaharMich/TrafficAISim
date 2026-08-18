// TrafficRegistrySubsystem.cpp

#include "TrafficRegistrySubsystem.h"

#include "Crosswalk.h"
#include "Components/SplineComponent.h"

void UTrafficRegistrySubsystem::RegisterLane(int32 LaneIndex, USplineComponent* Spline)
{
	if (Spline)
	{
		Lanes.Add(LaneIndex, Spline);
	}
}

void UTrafficRegistrySubsystem::RegisterFootpath(int32 FootpathIndex, USplineComponent* Spline)
{
	if (Spline)
	{
		Footpaths.Add(FootpathIndex, Spline);
	}
}

USplineComponent* UTrafficRegistrySubsystem::GetLane(int32 LaneIndex) const
{
	USplineComponent* const* Found = Lanes.Find(LaneIndex);
	return Found ? *Found : nullptr;
}

USplineComponent* UTrafficRegistrySubsystem::GetFootpath(int32 FootpathIndex) const
{
	USplineComponent* const* Found = Footpaths.Find(FootpathIndex);
	return Found ? *Found : nullptr;
}

void UTrafficRegistrySubsystem::RegisterCrosswalk(ACrosswalk* Crosswalk)
{
	if (Crosswalk)
	{
		Crosswalks.AddUnique(Crosswalk);
	}
}

ACrosswalk* UTrafficRegistrySubsystem::FindCrosswalkAhead(int32 LaneIndex, float FromDistance, float SearchAhead, float& OutDistanceToIt) const
{
	OutDistanceToIt = BIG_NUMBER;

	USplineComponent* Lane = GetLane(LaneIndex);
	if (!Lane)
	{
		return nullptr;
	}

	const float LaneLength = Lane->GetSplineLength();
	const bool bLoop = Lane->IsClosedLoop();

	ACrosswalk* Best = nullptr;

	for (ACrosswalk* Crosswalk : Crosswalks)
	{
		if (!IsValid(Crosswalk)) continue;

		float CrosswalkDistance = 0.f;
		if (!Crosswalk->GetDistanceOnLane(LaneIndex, CrosswalkDistance))
		{
			continue;
		}

		float Delta = CrosswalkDistance - FromDistance;
		if (bLoop && Delta < 0.f)
		{
			Delta += LaneLength; // wrapped around the loop
		}

		if (Delta >= 0.f && Delta <= SearchAhead && Delta < OutDistanceToIt)
		{
			OutDistanceToIt = Delta;
			Best = Crosswalk;
		}
	}

	return Best;
}

void UTrafficRegistrySubsystem::ReportVehicle(AActor* Vehicle, int32 LaneIndex, float DistanceAlongLane, float SpeedCms)
{
	if (!Vehicle) return;

	TArray<FLaneOccupant>& Occupants = LaneOccupants.FindOrAdd(LaneIndex);

	for (FLaneOccupant& Occupant : Occupants)
	{
		if (Occupant.Actor == Vehicle)
		{
			Occupant.DistanceAlongLane = DistanceAlongLane;
			Occupant.SpeedCms = SpeedCms;
			return;
		}
	}

	FLaneOccupant NewOccupant;
	NewOccupant.Actor = Vehicle;
	NewOccupant.DistanceAlongLane = DistanceAlongLane;
	NewOccupant.SpeedCms = SpeedCms;
	Occupants.Add(NewOccupant);
}

void UTrafficRegistrySubsystem::ForgetVehicle(AActor* Vehicle)
{
	for (TPair<int32, TArray<FLaneOccupant>>& Pair : LaneOccupants)
	{
		Pair.Value.RemoveAll([Vehicle](const FLaneOccupant& O) { return O.Actor == Vehicle; });
	}
}

float UTrafficRegistrySubsystem::GetTimeToArrival(int32 LaneIndex, float AtDistance) const
{
	const TArray<FLaneOccupant>* Occupants = LaneOccupants.Find(LaneIndex);
	USplineComponent* Lane = GetLane(LaneIndex);

	if (!Occupants || !Lane)
	{
		return BIG_NUMBER;
	}

	const float LaneLength = Lane->GetSplineLength();
	const bool bLoop = Lane->IsClosedLoop();

	float Soonest = BIG_NUMBER;

	for (const FLaneOccupant& Occupant : *Occupants)
	{
		if (!IsValid(Occupant.Actor)) continue;

		float Gap = AtDistance - Occupant.DistanceAlongLane;
		if (bLoop && Gap < 0.f)
		{
			Gap += LaneLength;
		}

		// Already past the crossing, or reversing. Not a threat.
		if (Gap < 0.f || Occupant.SpeedCms < 10.f)
		{
			continue;
		}

		Soonest = FMath::Min(Soonest, Gap / Occupant.SpeedCms);
	}

	return Soonest;
}
