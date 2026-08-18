// CrosswalkYieldComponent.cpp

#include "CrosswalkYieldComponent.h"

#include "Crosswalk.h"
#include "PurePursuitFollowerComponent.h"
#include "TrafficRegistrySubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UCrosswalkYieldComponent::UCrosswalkYieldComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void UCrosswalkYieldComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Follower = Owner->FindComponentByClass<UPurePursuitFollowerComponent>();
	}
}

void UCrosswalkYieldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Follower || Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	UTrafficRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	const float MyDistance = Follower->GetDistanceAlongPath();
	const float MySpeed = Owner->GetVelocity().Size();

	// Report our position so pedestrians can judge the gap.
	Registry->ReportVehicle(Owner, LaneIndex, MyDistance, MySpeed);

	float DistanceToCrosswalk = 0.f;
	ACrosswalk* Crosswalk = Registry->FindCrosswalkAhead(LaneIndex, MyDistance, ScanAhead, DistanceToCrosswalk);

	bYielding = false;
	Follower->ExternalSpeedLimitCms = -1.f; // no limit

	if (!Crosswalk || !Crosswalk->IsClaimed())
	{
		return;
	}

	const float StopLine = FMath::Max(DistanceToCrosswalk - StopOffset, 0.f);

	// Speed we can still be doing here and stop at the line comfortably: v = sqrt(2 a d)
	const float AllowedSpeed = FMath::Sqrt(2.f * ComfortDecel * StopLine);

	if (AllowedSpeed < MySpeed || StopLine < 100.f)
	{
		bYielding = true;
		Follower->ExternalSpeedLimitCms = AllowedSpeed;
	}

	if (bDrawDebug && bYielding)
	{
		DrawDebugSphere(GetWorld(), Crosswalk->GetActorLocation(), 120.f, 12, FColor::Red, false, 0.06f, 0, 3.f);
	}
}
