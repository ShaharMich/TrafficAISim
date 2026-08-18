// TrafficManager.h
// Spawns AI vehicles on lane splines and switches their simulation LOD
// based on distance to the nearest player. Exposes the stats the demo shows.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficManager.generated.h"

class AAITrafficVehicle;
class USplineComponent;

UCLASS()
class TRAFFICAISIM_API ATrafficManager : public AActor
{
	GENERATED_BODY()

public:
	ATrafficManager();

	virtual void Tick(float DeltaTime) override;

	/** Drag lane spline actors in here. Each must contain a USplineComponent. */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	TArray<AActor*> LaneActors;

	UPROPERTY(EditAnywhere, Category = "Traffic")
	TSubclassOf<AAITrafficVehicle> VehicleClass;

	UPROPERTY(EditAnywhere, Category = "Traffic")
	int32 VehicleCount = 20;

	/** Minimum gap between spawns along a lane, in cm. */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float SpawnSpacing = 2500.f;

	/** Inside this radius of any player, vehicles run full Chaos physics. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	float PhysicsRadius = 8000.f;

	/** Hysteresis band so vehicles at the boundary do not flip every frame. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	float LODHysteresis = 1500.f;

	/** Toggle to A/B the LOD system on camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	bool bLODEnabled = true;

	UPROPERTY(EditAnywhere, Category = "LOD")
	float LODUpdateInterval = 0.25f;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 GetPhysicsVehicleCount() const { return PhysicsCount; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 GetKinematicVehicleCount() const { return KinematicCount; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<AAITrafficVehicle*> Vehicles;
	UPROPERTY() TArray<USplineComponent*> Lanes;

	int32 PhysicsCount = 0;
	int32 KinematicCount = 0;
	float TimeSinceLODUpdate = 0.f;

	void SpawnTraffic();
	void UpdateLOD();
	float DistanceToNearestPlayer(const FVector& Location) const;
};
