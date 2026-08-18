// TrafficRegistrySubsystem.h
// Central lookup so pedestrians, vehicles and crosswalks can find each other
// without holding direct references or doing world-wide overlap queries.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrafficRegistrySubsystem.generated.h"

class USplineComponent;
class ACrosswalk;
class AAITrafficVehicle;

/** A vehicle's position along its lane, refreshed each tick by the vehicle itself. */
USTRUCT()
struct FLaneOccupant
{
	GENERATED_BODY()

	UPROPERTY() AActor* Actor = nullptr;
	float DistanceAlongLane = 0.f;
	float SpeedCms = 0.f;
};

UCLASS()
class TRAFFICAISIM_API UTrafficRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---- Paths ----
	void RegisterLane(int32 LaneIndex, USplineComponent* Spline);
	void RegisterFootpath(int32 FootpathIndex, USplineComponent* Spline);

	USplineComponent* GetLane(int32 LaneIndex) const;
	USplineComponent* GetFootpath(int32 FootpathIndex) const;

	const TMap<int32, USplineComponent*>& GetAllLanes() const { return Lanes; }
	const TMap<int32, USplineComponent*>& GetAllFootpaths() const { return Footpaths; }

	// ---- Crosswalks ----
	void RegisterCrosswalk(ACrosswalk* Crosswalk);

	const TArray<ACrosswalk*>& GetAllCrosswalks() const { return Crosswalks; }

	/**
	 * Nearest crosswalk ahead on this lane, within SearchAhead cm.
	 * Returns nullptr if none. OutDistanceToIt is measured along the lane.
	 */
	ACrosswalk* FindCrosswalkAhead(int32 LaneIndex, float FromDistance, float SearchAhead, float& OutDistanceToIt) const;

	// ---- Vehicle positions ----
	/** Called by each vehicle every tick so crosswalks can judge gaps cheaply. */
	void ReportVehicle(AActor* Vehicle, int32 LaneIndex, float DistanceAlongLane, float SpeedCms);
	void ForgetVehicle(AActor* Vehicle);

	/**
	 * Seconds until the soonest vehicle on LaneIndex reaches AtDistance.
	 * Returns BIG_NUMBER when the lane is clear.
	 */
	float GetTimeToArrival(int32 LaneIndex, float AtDistance) const;

private:
	UPROPERTY() TMap<int32, USplineComponent*> Lanes;
	UPROPERTY() TMap<int32, USplineComponent*> Footpaths;
	UPROPERTY() TArray<ACrosswalk*> Crosswalks;

	/** LaneIndex -> vehicles currently on it. */
	TMap<int32, TArray<FLaneOccupant>> LaneOccupants;
};
