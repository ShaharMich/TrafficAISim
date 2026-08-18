// AITrafficVehicle.h
// Wheeled vehicle pawn that can run either full Chaos physics or a cheap
// kinematic spline follow, switched at runtime by ATrafficManager.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "AITrafficVehicle.generated.h"

class UPurePursuitFollowerComponent;
class USplineComponent;

UENUM(BlueprintType)
enum class EVehicleSimLOD : uint8
{
	/** Full Chaos wheeled physics. Used near players. */
	Physics,
	/** Transform-driven follow along the spline. Cheap, no physics solver cost. */
	Kinematic
};

/** Everything a client needs to reproduce this vehicle's motion without transform updates. */
USTRUCT()
struct FVehicleIntent
{
	GENERATED_BODY()

	UPROPERTY() int32 LaneIndex = 0;
	UPROPERTY() float DistanceAlongPath = 0.f;
	UPROPERTY() float SpeedCms = 0.f;
	UPROPERTY() float ServerTimestamp = 0.f;
};

UCLASS()
class TRAFFICAISIM_API AAITrafficVehicle : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	AAITrafficVehicle();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Traffic")
	void InitialiseOnLane(USplineComponent* InSpline, int32 InLaneIndex, float StartDistance);

	/**
	 * Request a simulation level. A switch into Physics is refused while the
	 * vehicle is intersecting another, so call this repeatedly; the manager does.
	 */
	UFUNCTION(BlueprintCallable, Category = "Traffic")
	void SetSimLOD(EVehicleSimLOD NewLOD);

	UFUNCTION(BlueprintCallable, Category = "Traffic")
	EVehicleSimLOD GetSimLOD() const { return CurrentLOD; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	UPurePursuitFollowerComponent* Follower = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	class UCrosswalkYieldComponent* Yield = nullptr;

	/**
	 * Gap in cm a kinematic vehicle keeps behind the one ahead. Kinematic cars
	 * have no collision, so without this they drive through each other and then
	 * explode the moment one of them switches back to physics.
	 */
	UPROPERTY(EditAnywhere, Category = "LOD")
	float MinKinematicGap = 700.f;

	/** Half extent of the clearance test used before switching into physics. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	FVector PhysicsSwitchClearance = FVector(240.f, 100.f, 70.f);

	/**
	 * When true, kinematic vehicles stop replicating their transform and instead
	 * replicate a small intent struct that clients replay locally.
	 * This is the bandwidth story: cost per vehicle stops scaling with tick rate.
	 */
	UPROPERTY(EditAnywhere, Category = "Networking")
	bool bReplicateIntentOnly = true;

	/** How often to push intent when in intent-only mode. */
	UPROPERTY(EditAnywhere, Category = "Networking")
	float IntentUpdateInterval = 1.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(ReplicatedUsing = OnRep_Intent)
	FVehicleIntent ReplicatedIntent;

	UFUNCTION()
	void OnRep_Intent();

private:
	EVehicleSimLOD CurrentLOD = EVehicleSimLOD::Physics;

	UPROPERTY() USplineComponent* Lane = nullptr;
	int32 LaneIndex = 0;

	/** Local dead-reckoned position used by kinematic mode on both server and clients. */
	float KinematicDistance = 0.f;
	float KinematicSpeedCms = 0.f;
	float TimeSinceIntentPush = 0.f;
	/** Height from the spline to the vehicle origin when resting. Measure it, don't guess. */
	UPROPERTY(EditAnywhere, Category = "LOD")
	float KinematicHeightOffset = 0;

	/** Engine, transmission, steering and chassis defaults. Called from the constructor. */
	void ConfigureVehicleDefaults();

	/** True if turning physics on right now would resolve an intersection violently. */
	bool IsBlockedForPhysicsSwitch() const;

	/** Push our lane position to the registry so other vehicles can see us. */
	void ReportToRegistry();

	void TickKinematic(float DeltaTime);
	void PushIntent();
};