// CrosswalkYieldComponent.h
// Sits on an AI vehicle. Each tick it looks ahead on its lane for a claimed
// crossing and, if it finds one, feeds a speed ceiling into the steering
// component. Keeping this separate means the follower knows nothing about
// pedestrians, and anything else that needs to slow a car (a siren, a closed
// road, a scripted event) can use the same channel.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrosswalkYieldComponent.generated.h"

class UPurePursuitFollowerComponent;

UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class TRAFFICAISIM_API UCrosswalkYieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCrosswalkYieldComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Which lane the owning vehicle is on. Set by AAITrafficVehicle::InitialiseOnLane. */
	void SetLaneIndex(int32 InLaneIndex) { LaneIndex = InLaneIndex; }

	/** How far ahead to look for crossings. */
	UPROPERTY(EditAnywhere, Category = "Yield") float ScanAhead = 4000.f;

	/** Stop this far short of the crossing centre. */
	UPROPERTY(EditAnywhere, Category = "Yield") float StopOffset = 500.f;

	/** Comfortable deceleration used to work out when to start braking, cm/s^2. */
	UPROPERTY(EditAnywhere, Category = "Yield") float ComfortDecel = 450.f;

	UPROPERTY(EditAnywhere, Category = "Debug") bool bDrawDebug = false;

	UFUNCTION(BlueprintCallable, Category = "Yield")
	bool IsYielding() const { return bYielding; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() UPurePursuitFollowerComponent* Follower = nullptr;
	int32 LaneIndex = 0;
	bool bYielding = false;
};
