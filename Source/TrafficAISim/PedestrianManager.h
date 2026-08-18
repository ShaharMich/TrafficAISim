// PedestrianManager.h
// Spawns pedestrians on footpath splines and throttles their tick rate by
// distance to the nearest player. Same LOD idea as the vehicles, but the
// lever here is tick frequency rather than physics, because these agents
// were never running physics in the first place.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PedestrianManager.generated.h"

class APedestrianAgent;
class USplineComponent;

UCLASS()
class TRAFFICAISIM_API APedestrianManager : public AActor
{
	GENERATED_BODY()

public:
	APedestrianManager();

	virtual void Tick(float DeltaTime) override;

	/** Actors containing footpath splines. Route them through crosswalk zones. */
	UPROPERTY(EditAnywhere, Category = "Pedestrians")
	TArray<AActor*> FootpathActors;

	UPROPERTY(EditAnywhere, Category = "Pedestrians")
	TSubclassOf<APedestrianAgent> PedestrianClass;

	UPROPERTY(EditAnywhere, Category = "Pedestrians")
	int32 PedestrianCount = 120;

	UPROPERTY(EditAnywhere, Category = "Pedestrians")
	float SpawnSpacing = 400.f;

	// ---- LOD ----
	/** Inside this radius pedestrians tick every frame. */
	UPROPERTY(EditAnywhere, Category = "LOD") float NearRadius = 5000.f;
	/** Between near and far they tick at MidTickInterval. */
	UPROPERTY(EditAnywhere, Category = "LOD") float FarRadius = 15000.f;
	UPROPERTY(EditAnywhere, Category = "LOD") float MidTickInterval = 0.1f;
	UPROPERTY(EditAnywhere, Category = "LOD") float FarTickInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD") bool bLODEnabled = true;
	UPROPERTY(EditAnywhere, Category = "LOD") float LODUpdateInterval = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Stats") int32 GetNearCount() const { return NearCount; }
	UFUNCTION(BlueprintCallable, Category = "Stats") int32 GetMidCount() const { return MidCount; }
	UFUNCTION(BlueprintCallable, Category = "Stats") int32 GetFarCount() const { return FarCount; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TArray<APedestrianAgent*> Pedestrians;
	UPROPERTY() TArray<USplineComponent*> Paths;

	int32 NearCount = 0;
	int32 MidCount = 0;
	int32 FarCount = 0;
	float TimeSinceLODUpdate = 0.f;

	void SpawnPedestrians();
	void UpdateLOD();
	float DistanceToNearestPlayer(const FVector& Location) const;
};
