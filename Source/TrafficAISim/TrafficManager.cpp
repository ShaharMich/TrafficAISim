// TrafficManager.cpp

#include "TrafficManager.h"

#include "AITrafficVehicle.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ATrafficManager::ATrafficManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false; // Server-only director. Vehicles themselves replicate.
}

void ATrafficManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	for (AActor* LaneActor : LaneActors)
	{
		if (!LaneActor) continue;
		if (USplineComponent* Spline = LaneActor->FindComponentByClass<USplineComponent>())
		{
			Lanes.Add(Spline);
		}
	}

	if (Lanes.Num() == 0 || !VehicleClass)
	{
		UE_LOG(LogTemp, Error, TEXT("TrafficManager: assign LaneActors with splines and a VehicleClass."));
		return;
	}

	SpawnTraffic();
}

void ATrafficManager::SpawnTraffic()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TArray<float> NextFreeDistance;
	NextFreeDistance.Init(0.f, Lanes.Num());

	for (int32 i = 0; i < VehicleCount; ++i)
	{
		const int32 LaneIndex = i % Lanes.Num();
		USplineComponent* Lane = Lanes[LaneIndex];

		const float Length = Lane->GetSplineLength();
		const float Distance = FMath::Fmod(NextFreeDistance[LaneIndex], Length);
		NextFreeDistance[LaneIndex] += SpawnSpacing;

		const FVector Location = Lane->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + FVector(0.f, 0.f, 60.f);
		const FRotator Rotation = Lane->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

		AAITrafficVehicle* Vehicle = World->SpawnActor<AAITrafficVehicle>(
			VehicleClass, Location, FRotator(0.f, Rotation.Yaw, 0.f), Params);

		if (Vehicle)
		{
			Vehicle->InitialiseOnLane(Lane, LaneIndex, Distance);
			Vehicles.Add(Vehicle);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("TrafficManager: spawned %d vehicles across %d lanes."), Vehicles.Num(), Lanes.Num());
}

void ATrafficManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	TimeSinceLODUpdate += DeltaTime;
	if (TimeSinceLODUpdate >= LODUpdateInterval)
	{
		TimeSinceLODUpdate = 0.f;
		UpdateLOD();
	}
}

void ATrafficManager::UpdateLOD()
{
	PhysicsCount = 0;
	KinematicCount = 0;

	for (AAITrafficVehicle* Vehicle : Vehicles)
	{
		if (!IsValid(Vehicle)) continue;

		EVehicleSimLOD Desired = EVehicleSimLOD::Physics;

		if (bLODEnabled)
		{
			const float Distance = DistanceToNearestPlayer(Vehicle->GetActorLocation());
			const bool bCurrentlyPhysics = (Vehicle->GetSimLOD() == EVehicleSimLOD::Physics);

			// Hysteresis: widen the band you must cross to leave your current state.
			const float Threshold = bCurrentlyPhysics ? (PhysicsRadius + LODHysteresis) : PhysicsRadius;
			Desired = (Distance <= Threshold) ? EVehicleSimLOD::Physics : EVehicleSimLOD::Kinematic;
		}

		Vehicle->SetSimLOD(Desired);

		if (Desired == EVehicleSimLOD::Physics) ++PhysicsCount; else ++KinematicCount;
	}
}

float ATrafficManager::DistanceToNearestPlayer(const FVector& Location) const
{
	float Best = TNumericLimits<float>::Max();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PC = It->Get())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				Best = FMath::Min(Best, FVector::Dist(Location, Pawn->GetActorLocation()));
			}
		}
	}

	return Best;
}
