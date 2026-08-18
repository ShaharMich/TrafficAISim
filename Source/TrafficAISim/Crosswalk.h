// Crosswalk.h
// A place where a footpath crosses one or more vehicle lanes.
// Holds the arbitration between the two: who has claimed the crossing,
// and whether there is a gap in traffic big enough to step into.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Crosswalk.generated.h"

class UBoxComponent;

UCLASS()
class TRAFFICAISIM_API ACrosswalk : public AActor
{
	GENERATED_BODY()

public:
	ACrosswalk();

	virtual void Tick(float DeltaTime) override;

	/** Extent of the crossing. Must overlap the lane splines it governs. */
	UPROPERTY(VisibleAnywhere, Category = "Crosswalk")
	UBoxComponent* Zone = nullptr;

	/** A pedestrian must clear the crossing within this long or we assume it despawned. */
	UPROPERTY(EditAnywhere, Category = "Crosswalk")
	float ClaimTimeout = 12.f;

	/** Gap in seconds a pedestrian wants before stepping out. */
	UPROPERTY(EditAnywhere, Category = "Crosswalk")
	float RequiredGapSeconds = 4.f;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	/** Distance along LaneIndex where this crossing sits. False if it does not touch that lane. */
	bool GetDistanceOnLane(int32 LaneIndex, float& OutDistance) const;

	/** Distance along FootpathIndex where this crossing sits. */
	bool GetDistanceOnFootpath(int32 FootpathIndex, float& OutDistance) const;

	/** True while any pedestrian has committed to crossing. Vehicles stop for this. */
	UFUNCTION(BlueprintCallable, Category = "Crosswalk")
	bool IsClaimed() const;

	/** Is there a long enough gap on every lane this crossing spans. */
	UFUNCTION(BlueprintCallable, Category = "Crosswalk")
	bool IsSafeToCross() const;

	void Claim(AActor* Pedestrian);
	void Release(AActor* Pedestrian);

protected:
	virtual void BeginPlay() override;

private:
	/** Populated at BeginPlay by projecting every registered spline onto this zone. */
	TMap<int32, float> LaneDistances;
	TMap<int32, float> FootpathDistances;

	struct FClaim
	{
		TWeakObjectPtr<AActor> Pedestrian;
		float TimeClaimed = 0.f;
	};

	TArray<FClaim> Claims;

	void MapOverlappingSplines();
};
