// PedestrianManager.cpp

#include "PedestrianManager.h"

#include "PedestrianAgent.h"
#include "TrafficRegistrySubsystem.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

APedestrianManager::APedestrianManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
}

void APedestrianManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	UTrafficRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>();

	for (AActor* PathActor : FootpathActors)
	{
		if (!PathActor) continue;
		if (USplineComponent* Spline = PathActor->FindComponentByClass<USplineComponent>())
		{
			const int32 Index = Paths.Add(Spline);
			if (Registry)
			{
				Registry->RegisterFootpath(Index, Spline);
			}
		}
	}

	if (Paths.Num() == 0 || !PedestrianClass)
	{
		UE_LOG(LogTemp, Error, TEXT("PedestrianManager: assign FootpathActors with splines and a PedestrianClass."));
		return;
	}

	SpawnPedestrians();
}

void APedestrianManager::SpawnPedestrians()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TArray<float> NextFree;
	NextFree.Init(0.f, Paths.Num());

	for (int32 i = 0; i < PedestrianCount; ++i)
	{
		const int32 PathIndex = i % Paths.Num();
		USplineComponent* Path = Paths[PathIndex];

		const float Length = Path->GetSplineLength();
		const float Distance = FMath::Fmod(NextFree[PathIndex] + FMath::FRandRange(0.f, SpawnSpacing * 0.5f), Length);
		NextFree[PathIndex] += SpawnSpacing;

		const FVector Location = Path->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + FVector(0.f, 0.f, 88.f);

		APedestrianAgent* Agent = World->SpawnActor<APedestrianAgent>(PedestrianClass, Location, FRotator::ZeroRotator, Params);
		if (Agent)
		{
			Agent->InitialiseOnFootpath(Path, PathIndex, Distance);
			Pedestrians.Add(Agent);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PedestrianManager: spawned %d pedestrians across %d footpaths."), Pedestrians.Num(), Paths.Num());
}

void APedestrianManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority()) return;

	TimeSinceLODUpdate += DeltaTime;
	if (TimeSinceLODUpdate >= LODUpdateInterval)
	{
		TimeSinceLODUpdate = 0.f;
		UpdateLOD();
	}
}

void APedestrianManager::UpdateLOD()
{
	NearCount = MidCount = FarCount = 0;

	for (APedestrianAgent* Agent : Pedestrians)
	{
		if (!IsValid(Agent)) continue;

		if (!bLODEnabled)
		{
			Agent->SetActorTickInterval(0.f);
			Agent->SetNetUpdateFrequency(10.f);
			++NearCount;
			continue;
		}

		const float Distance = DistanceToNearestPlayer(Agent->GetActorLocation());

		if (Distance <= NearRadius)
		{
			Agent->SetActorTickInterval(0.f); // every frame
			Agent->SetNetUpdateFrequency(10.f);
			++NearCount;
		}
		else if (Distance <= FarRadius)
		{
			Agent->SetActorTickInterval(MidTickInterval);
			Agent->SetNetUpdateFrequency(4.f);
			++MidCount;
		}
		else
		{
			// Still simulating, just coarsely. Crowds keep flowing off camera,
			// so nothing pops into existence when the player turns around.
			Agent->SetActorTickInterval(FarTickInterval);
			Agent->SetNetUpdateFrequency(1.f);
			++FarCount;
		}
	}
}

float APedestrianManager::DistanceToNearestPlayer(const FVector& Location) const
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
