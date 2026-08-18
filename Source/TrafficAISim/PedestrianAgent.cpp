// PedestrianAgent.cpp

#include "PedestrianAgent.h"

#include "Crosswalk.h"
#include "TrafficRegistrySubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

APedestrianAgent::APedestrianAgent()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToAllChannels(ECR_Overlap);
	Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	RootComponent = Capsule;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.f);
	SetNetCullDistanceSquared(100000000.f); // 100m
}

void APedestrianAgent::InitialiseOnFootpath(USplineComponent* InFootpath, int32 InFootpathIndex, float StartDistance)
{
	Footpath = InFootpath;
	FootpathIndex = InFootpathIndex;
	Distance = StartDistance;

	// Small per-agent speed variance. Without it a crowd moves like one object.
	SpeedScale = FMath::FRandRange(1.f - SpeedVariance, 1.f + SpeedVariance);
	CurrentSpeed = WalkSpeedCms * SpeedScale;

	ApplyTransformFromPath();
	ScanForCrossing();
}

void APedestrianAgent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (PendingCrosswalk.IsValid())
	{
		PendingCrosswalk->Release(this);
	}
	Super::EndPlay(Reason);
}

void APedestrianAgent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Footpath || !HasAuthority())
	{
		return;
	}

	switch (State)
	{
	case EPedestrianState::Walking:          TickWalking(DeltaTime); break;
	case EPedestrianState::WaitingAtCrossing: TickWaiting(DeltaTime); break;
	case EPedestrianState::Crossing:          TickCrossing(DeltaTime); break;
	}

	if (bDrawDebug)
	{
		const FColor Colour =
			State == EPedestrianState::Crossing ? FColor::Red :
			State == EPedestrianState::WaitingAtCrossing ? FColor::Yellow : FColor::Cyan;

		DrawDebugCapsule(GetWorld(), GetActorLocation(), 88.f, 34.f, FQuat::Identity, Colour, false, -1.f, 0, 2.f);
	}
}

void APedestrianAgent::TickWalking(float DeltaTime)
{
	if (!PendingCrosswalk.IsValid())
	{
		ScanForCrossing();
	}

	// Stop short of a crossing we have not yet been cleared for.
	if (PendingCrosswalk.IsValid())
	{
		const float StopAt = PendingCrosswalkDistance - StopBuffer;
		if (Distance >= StopAt - 5.f)
		{
			Distance = StopAt;
			CurrentSpeed = 0.f;
			WaitTimer = 0.f;
			State = EPedestrianState::WaitingAtCrossing;
			ApplyTransformFromPath();
			return;
		}
	}

	Advance(DeltaTime, WalkSpeedCms * SpeedScale);
}

void APedestrianAgent::TickWaiting(float DeltaTime)
{
	ACrosswalk* Crosswalk = PendingCrosswalk.Get();
	if (!Crosswalk)
	{
		State = EPedestrianState::Walking;
		return;
	}

	WaitTimer += DeltaTime;

	// Either traffic gave us a gap, or we ran out of patience and step out anyway.
	// The patience fallback is what stops a busy lane from freezing the sidewalk.
	const bool bGo = Crosswalk->IsSafeToCross() || WaitTimer >= MaxPatience;

	if (bGo)
	{
		Crosswalk->Claim(this);
		State = EPedestrianState::Crossing;
	}
}

void APedestrianAgent::TickCrossing(float DeltaTime)
{
	ACrosswalk* Crosswalk = PendingCrosswalk.Get();

	Advance(DeltaTime, CrossSpeedCms * SpeedScale);

	// Clear of the far kerb.
	if (Distance >= PendingCrosswalkDistance + ClearBuffer)
	{
		if (Crosswalk)
		{
			Crosswalk->Release(this);
		}
		PendingCrosswalk = nullptr;
		State = EPedestrianState::Walking;
		ScanForCrossing();
	}
}

void APedestrianAgent::Advance(float DeltaTime, float TargetSpeed)
{
	CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, 4.f);

	const float Length = Footpath->GetSplineLength();
	Distance += CurrentSpeed * DeltaTime;

	if (Footpath->IsClosedLoop())
	{
		if (Distance >= Length)
		{
			Distance = FMath::Fmod(Distance, Length);
			PendingCrosswalk = nullptr; // new lap, rescan
			ScanForCrossing();
		}
	}
	else
	{
		Distance = FMath::Min(Distance, Length);
	}

	ApplyTransformFromPath();
}

void APedestrianAgent::ApplyTransformFromPath()
{
	if (!Footpath) return;

	const FVector Location = Footpath->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	const FVector Direction = Footpath->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

	SetActorLocationAndRotation(
		Location + FVector(0.f, 0.f, 88.f),
		FRotator(0.f, Direction.Rotation().Yaw, 0.f),
		false, nullptr, ETeleportType::TeleportPhysics);
}

void APedestrianAgent::ScanForCrossing()
{
	UTrafficRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>() : nullptr;
	if (!Registry || !Footpath) return;

	const float Length = Footpath->GetSplineLength();
	const bool bLoop = Footpath->IsClosedLoop();

	ACrosswalk* Nearest = nullptr;
	float NearestDistance = BIG_NUMBER;
	float NearestAhead = BIG_NUMBER;

	for (ACrosswalk* Candidate : Registry->GetAllCrosswalks())
	{
		if (!IsValid(Candidate)) continue;

		float OnFootpath = 0.f;
		if (!Candidate->GetDistanceOnFootpath(FootpathIndex, OnFootpath)) continue;

		float Ahead = OnFootpath - Distance;
		if (bLoop && Ahead < 0.f) Ahead += Length;

		if (Ahead >= 0.f && Ahead < NearestAhead)
		{
			NearestAhead = Ahead;
			NearestDistance = OnFootpath;
			Nearest = Candidate;
		}
	}

	PendingCrosswalk = Nearest;
	PendingCrosswalkDistance = NearestDistance;
}
