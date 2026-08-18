// Crosswalk.cpp

#include "Crosswalk.h"

#include "TrafficRegistrySubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ACrosswalk::ACrosswalk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // arbitration does not need frame rate

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(400.f, 200.f, 200.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Zone;
}

void ACrosswalk::BeginPlay()
{
	Super::BeginPlay();

	if (UTrafficRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>())
	{
		Registry->RegisterCrosswalk(this);
	}

	// Deferred one frame so managers have registered their splines first.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimerForNextTick([this]() { MapOverlappingSplines(); });
}

void ACrosswalk::MapOverlappingSplines()
{
	UTrafficRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>() : nullptr;
	if (!Registry || !Zone)
	{
		return;
	}

	const FVector Centre = GetActorLocation();
	const float Radius = Zone->GetScaledBoxExtent().Size2D();

	auto MapSet = [&](const TMap<int32, USplineComponent*>& Splines, TMap<int32, float>& Out)
	{
		for (const TPair<int32, USplineComponent*>& Pair : Splines)
		{
			USplineComponent* Spline = Pair.Value;
			if (!Spline) continue;

			const float Key = Spline->FindInputKeyClosestToWorldLocation(Centre);
			const float Distance = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
			const FVector ClosestPoint = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

			if (FVector::Dist2D(ClosestPoint, Centre) <= Radius)
			{
				Out.Add(Pair.Key, Distance);
			}
		}
	};

	MapSet(Registry->GetAllLanes(), LaneDistances);
	MapSet(Registry->GetAllFootpaths(), FootpathDistances);

	UE_LOG(LogTemp, Log, TEXT("Crosswalk %s spans %d lanes and %d footpaths."),
		*GetName(), LaneDistances.Num(), FootpathDistances.Num());
}

bool ACrosswalk::GetDistanceOnLane(int32 LaneIndex, float& OutDistance) const
{
	if (const float* Found = LaneDistances.Find(LaneIndex))
	{
		OutDistance = *Found;
		return true;
	}
	return false;
}

bool ACrosswalk::GetDistanceOnFootpath(int32 FootpathIndex, float& OutDistance) const
{
	if (const float* Found = FootpathDistances.Find(FootpathIndex))
	{
		OutDistance = *Found;
		return true;
	}
	return false;
}

bool ACrosswalk::IsClaimed() const
{
	for (const FClaim& Claim : Claims)
	{
		if (Claim.Pedestrian.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool ACrosswalk::IsSafeToCross() const
{
	UTrafficRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		return true;
	}

	// Once one pedestrian has committed, the rest follow. This is what makes
	// groups cross together instead of trickling out one at a time.
	if (IsClaimed())
	{
		return true;
	}

	for (const TPair<int32, float>& Pair : LaneDistances)
	{
		if (Registry->GetTimeToArrival(Pair.Key, Pair.Value) < RequiredGapSeconds)
		{
			return false;
		}
	}

	return true;
}

void ACrosswalk::Claim(AActor* Pedestrian)
{
	if (!Pedestrian) return;

	for (const FClaim& Existing : Claims)
	{
		if (Existing.Pedestrian.Get() == Pedestrian)
		{
			return;
		}
	}

	FClaim NewClaim;
	NewClaim.Pedestrian = Pedestrian;
	NewClaim.TimeClaimed = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Claims.Add(NewClaim);
}

void ACrosswalk::Release(AActor* Pedestrian)
{
	Claims.RemoveAll([Pedestrian](const FClaim& C)
	{
		return !C.Pedestrian.IsValid() || C.Pedestrian.Get() == Pedestrian;
	});
}

void ACrosswalk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Never let a dead or stuck pedestrian hold traffic forever.
	Claims.RemoveAll([Now, this](const FClaim& C)
	{
		return !C.Pedestrian.IsValid() || (Now - C.TimeClaimed) > ClaimTimeout;
	});

	if (bDrawDebug)
	{
		const FColor Colour = IsClaimed() ? FColor::Red : (IsSafeToCross() ? FColor::Green : FColor::Yellow);
		DrawDebugBox(GetWorld(), GetActorLocation(), Zone->GetScaledBoxExtent(), GetActorQuat(), Colour, false, 0.11f, 0, 4.f);
	}
}
