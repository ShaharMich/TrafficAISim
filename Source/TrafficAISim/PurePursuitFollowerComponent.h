// PurePursuitFollowerComponent.h
// Steering + speed controller for a spline-following AI vehicle.
// Rename VEHICLEAIDEMO_API to YOURMODULE_API before compiling.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PurePursuitFollowerComponent.generated.h"

class USplineComponent;
class UChaosWheeledVehicleMovementComponent;

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class TRAFFICAISIM_API UPurePursuitFollowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPurePursuitFollowerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Assign the lane this vehicle drives, and where along it to start. */
	UFUNCTION(BlueprintCallable, Category = "Path")
	void SetPath(USplineComponent* InSpline, float StartDistance);

	UFUNCTION(BlueprintCallable, Category = "Path")
	float GetDistanceAlongPath() const { return CachedDistance; }

	UFUNCTION(BlueprintCallable, Category = "Path")
	USplineComponent* GetPath() const { return Path.Get(); }

	/** Last computed target speed in cm/s. Used by the kinematic LOD so both modes agree. */
	float GetTargetSpeedCms() const { return LastTargetSpeedCms; }

	// ---- Steering tuning ----
	/** Lookahead at standstill. Too small oscillates, too large cuts corners. */
	UPROPERTY(EditAnywhere, Category = "Steering") float BaseLookahead = 600.f;
	/** Extra lookahead per cm/s of forward speed. */
	UPROPERTY(EditAnywhere, Category = "Steering") float LookaheadPerSpeed = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Steering") float MaxLookahead = 3000.f;
	/** Front-to-rear axle distance in cm. Read it off your wheel setup. */
	UPROPERTY(EditAnywhere, Category = "Steering") float WheelBase = 280.f;
	UPROPERTY(EditAnywhere, Category = "Steering") float MaxSteerAngleDeg = 35.f;
	UPROPERTY(EditAnywhere, Category = "Steering") float SteeringInterpSpeed = 6.f;

	// ---- Speed tuning ----
	UPROPERTY(EditAnywhere, Category = "Speed") float CruiseSpeedKph = 55.f;
	UPROPERTY(EditAnywhere, Category = "Speed") float MinCornerSpeedKph = 18.f;
	/** Lateral acceleration budget in cm/s^2. Lower means slower cornering. */
	UPROPERTY(EditAnywhere, Category = "Speed") float LateralAccelLimit = 700.f;
	/** How far ahead to sample curvature when planning corner entry speed. */
	UPROPERTY(EditAnywhere, Category = "Speed") float CurvatureProbeAhead = 1200.f;
	UPROPERTY(EditAnywhere, Category = "Speed") float Kp = 0.0025f;
	UPROPERTY(EditAnywhere, Category = "Speed") float Ki = 0.0002f;
	UPROPERTY(EditAnywhere, Category = "Speed") float Kd = 0.0004f;

	// ---- Avoidance ----
	UPROPERTY(EditAnywhere, Category = "Avoidance") float ProbeDistance = 1600.f;
	UPROPERTY(EditAnywhere, Category = "Avoidance") float ProbeRadius = 110.f;

	/**
	 * Ceiling in cm/s imposed from outside (crosswalk yield, road closure, siren).
	 * Negative means no limit. Whoever sets it must clear it when it no longer applies.
	 */
	float ExternalSpeedLimitCms = -1.f;

	UPROPERTY(EditAnywhere, Category = "Debug") bool bDrawDebug = false;

	// Read-only telemetry for the debug HUD.
	float DebugCrossTrackError = 0.f;
	FVector DebugLookaheadPoint = FVector::ZeroVector;

protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<USplineComponent> Path;

	UPROPERTY() UChaosWheeledVehicleMovementComponent* VehicleMovement = nullptr;

	float CachedDistance = 0.f;
	float SmoothedSteering = 0.f;
	float SpeedErrorIntegral = 0.f;
	float LastSpeedError = 0.f;
	float LastTargetSpeedCms = 0.f;

	/** Project the vehicle onto the spline and update CachedDistance. */
	void UpdateDistanceAlongPath(const FVector& WorldLocation);

	/** Worst curvature over the next CurvatureProbeAhead cm, as 1/radius. */
	float SampleUpcomingCurvature() const;

	/** 0..1 multiplier on target speed from a forward sweep. */
	float ComputeAvoidanceScale() const;
};
