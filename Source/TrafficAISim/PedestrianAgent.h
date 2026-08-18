// PedestrianAgent.h
// A deliberately lightweight pedestrian. No CharacterMovementComponent,
// no NavMesh query per agent, no physics. Follows a footpath spline and
// negotiates crosswalks. This is what lets the count go into the hundreds.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PedestrianAgent.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class USplineComponent;
class ACrosswalk;

UENUM(BlueprintType)
enum class EPedestrianState : uint8
{
	Walking,
	WaitingAtCrossing,
	Crossing
};

UCLASS()
class TRAFFICAISIM_API APedestrianAgent : public AActor
{
	GENERATED_BODY()

public:
	APedestrianAgent();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION(BlueprintCallable, Category = "Pedestrian")
	void InitialiseOnFootpath(USplineComponent* InFootpath, int32 InFootpathIndex, float StartDistance);

	UPROPERTY(VisibleAnywhere, Category = "Pedestrian")
	UCapsuleComponent* Capsule = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Pedestrian")
	USkeletalMeshComponent* Body = nullptr;

	UPROPERTY(EditAnywhere, Category = "Pedestrian") float WalkSpeedCms = 140.f;
	UPROPERTY(EditAnywhere, Category = "Pedestrian") float CrossSpeedCms = 190.f;
	UPROPERTY(EditAnywhere, Category = "Pedestrian") float SpeedVariance = 0.25f;

	/** How far before the crossing to stop and look. */
	UPROPERTY(EditAnywhere, Category = "Crossing") float StopBuffer = 160.f;

	/** How far past the crossing counts as clear. */
	UPROPERTY(EditAnywhere, Category = "Crossing") float ClearBuffer = 220.f;

	/** After waiting this long, step out regardless. Prevents permanent deadlock. */
	UPROPERTY(EditAnywhere, Category = "Crossing") float MaxPatience = 9.f;

	UPROPERTY(EditAnywhere, Category = "Debug") bool bDrawDebug = false;

	UFUNCTION(BlueprintCallable, Category = "Pedestrian")
	EPedestrianState GetState() const { return State; }

	float GetDistanceAlongPath() const { return Distance; }

private:
	UPROPERTY() USplineComponent* Footpath = nullptr;
	int32 FootpathIndex = 0;

	EPedestrianState State = EPedestrianState::Walking;

	float Distance = 0.f;
	float CurrentSpeed = 0.f;
	float SpeedScale = 1.f;
	float WaitTimer = 0.f;

	/** The crossing we are approaching or inside, plus where it sits on our path. */
	TWeakObjectPtr<ACrosswalk> PendingCrosswalk;
	float PendingCrosswalkDistance = 0.f;

	void TickWalking(float DeltaTime);
	void TickWaiting(float DeltaTime);
	void TickCrossing(float DeltaTime);

	void Advance(float DeltaTime, float TargetSpeed);
	void ApplyTransformFromPath();

	/** Look for the next crossing ahead on our footpath. */
	void ScanForCrossing();
};
