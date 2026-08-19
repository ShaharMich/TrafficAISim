// PurePursuitFollowerComponent.cpp

#include "PurePursuitFollowerComponent.h"

#include "Components/SplineComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TrafficRegistrySubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

UPurePursuitFollowerComponent::UPurePursuitFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false); // Control is server-side only. See README on intent replication.
}

void UPurePursuitFollowerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		VehicleMovement = Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	}
}

void UPurePursuitFollowerComponent::SetPath(USplineComponent* InSpline, float StartDistance)
{
	Path = InSpline;
	CachedDistance = StartDistance;
	SpeedErrorIntegral = 0.f;
	LastSpeedError = 0.f;
}

void UPurePursuitFollowerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || !Path.IsValid() || !VehicleMovement)
	{
		return;
	}

	// Only the authority drives. Clients either receive transforms or replay intent.
	if (Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FVector OwnerLocation = OwnerTransform.GetLocation();

	UpdateDistanceAlongPath(OwnerLocation);

	// Forward speed in cm/s, signed.
	const FVector Velocity = Owner->GetVelocity();
	const float ForwardSpeed = FVector::DotProduct(Velocity, OwnerTransform.GetUnitAxis(EAxis::X));

	// ---------------------------------------------------------------
	// Steering: pure pursuit.
	// Curvature to a lookahead point in vehicle space is k = 2y / d^2.
	// Steer angle for a bicycle model is atan(k * wheelbase).
	// ---------------------------------------------------------------
	const float Lookahead = FMath::Clamp(BaseLookahead + FMath::Abs(ForwardSpeed) * LookaheadPerSpeed, BaseLookahead, MaxLookahead);
	const float SplineLength = Path->GetSplineLength();
	const float TargetDistance = Path->IsClosedLoop()
		? FMath::Fmod(CachedDistance + Lookahead, SplineLength)
		: FMath::Min(CachedDistance + Lookahead, SplineLength);

	const FVector TargetWorld = Path->GetLocationAtDistanceAlongSpline(TargetDistance, ESplineCoordinateSpace::World);
	const FVector TargetLocal = OwnerTransform.InverseTransformPosition(TargetWorld);

	const float DistSq = FMath::Max(TargetLocal.SizeSquared2D(), 1.f);
	const float Curvature = (2.f * TargetLocal.Y) / DistSq;
	const float SteerAngleRad = FMath::Atan(Curvature * WheelBase);
	const float RawSteering = FMath::Clamp(FMath::RadiansToDegrees(SteerAngleRad) / MaxSteerAngleDeg, -1.f, 1.f);

	// Pure pursuit alone settles at an offset through sustained curves.
	// A Stanley-style cross-track term pulls it back onto the line.
	const FVector ClosestNow = Path->GetLocationAtDistanceAlongSpline(CachedDistance, ESplineCoordinateSpace::World);
	const FVector ToPath = OwnerTransform.InverseTransformPosition(ClosestNow);
	const float CrossTrack = ToPath.Y; // signed: positive means the path is to our right

	const float CrossTrackTerm = FMath::Clamp(CrossTrack * CrossTrackGain, -MaxCrossTrackCorrection, MaxCrossTrackCorrection);
	const float CombinedSteering = FMath::Clamp(RawSteering + CrossTrackTerm, -1.f, 1.f);

	SmoothedSteering = FMath::FInterpTo(SmoothedSteering, CombinedSteering, DeltaTime, SteeringInterpSpeed);
	VehicleMovement->SetSteeringInput(SmoothedSteering);

	// ---------------------------------------------------------------
	// Speed: curvature-limited target, then PID onto throttle or brake.
	// ---------------------------------------------------------------
	const float UpcomingCurvature = SampleUpcomingCurvature();
	float TargetSpeedCms = CruiseSpeedKph * 27.7778f; // kph to cm/s

	if (UpcomingCurvature > KINDA_SMALL_NUMBER)
	{
		// v = sqrt(a_lat / k)
		const float CornerLimit = FMath::Sqrt(LateralAccelLimit / UpcomingCurvature);
		TargetSpeedCms = FMath::Min(TargetSpeedCms, CornerLimit);
	}

	TargetSpeedCms = FMath::Max(TargetSpeedCms, MinCornerSpeedKph * 27.7778f);
	TargetSpeedCms *= ComputeAvoidanceScale();

	// External ceiling wins over everything, including the corner minimum.
	// This is the only path by which a vehicle is allowed to come to a full stop.
	if (ExternalSpeedLimitCms >= 0.f)
	{
		TargetSpeedCms = FMath::Min(TargetSpeedCms, ExternalSpeedLimitCms);
	}

	LastTargetSpeedCms = TargetSpeedCms;

	const float SpeedError = TargetSpeedCms - ForwardSpeed;
	SpeedErrorIntegral = FMath::Clamp(SpeedErrorIntegral + SpeedError * DeltaTime, -2000.f, 2000.f);
	const float Derivative = (DeltaTime > KINDA_SMALL_NUMBER) ? (SpeedError - LastSpeedError) / DeltaTime : 0.f;
	LastSpeedError = SpeedError;

	const float ControlOutput = (Kp * SpeedError) + (Ki * SpeedErrorIntegral) + (Kd * Derivative);

	VehicleMovement->SetThrottleInput(FMath::Clamp(ControlOutput, 0.f, 1.f));
	VehicleMovement->SetBrakeInput(FMath::Clamp(-ControlOutput, 0.f, 1.f));
	VehicleMovement->SetHandbrakeInput(false);

	// ---------------------------------------------------------------
	// Telemetry
	// ---------------------------------------------------------------
	const FVector ClosestOnPath = Path->GetLocationAtDistanceAlongSpline(CachedDistance, ESplineCoordinateSpace::World);
	DebugCrossTrackError = FVector::Dist2D(ClosestOnPath, OwnerLocation);
	DebugLookaheadPoint = TargetWorld;

	if (bDrawDebug)
	{
		const UWorld* World = GetWorld();
		DrawDebugSphere(World, TargetWorld, 40.f, 8, FColor::Green, false, -1.f, 0, 2.f);
		DrawDebugLine(World, OwnerLocation, TargetWorld, FColor::Green, false, -1.f, 0, 2.f);
		DrawDebugLine(World, OwnerLocation, ClosestOnPath, FColor::Red, false, -1.f, 0, 2.f);
	}
}

void UPurePursuitFollowerComponent::UpdateDistanceAlongPath(const FVector& WorldLocation)
{
	if (!Path.IsValid())
	{
		return;
	}

	// Straightforward projection. Fine for tens of vehicles.
	// At hundreds, replace with a segment-local search seeded from CachedDistance.
	const float InputKey = Path->FindInputKeyClosestToWorldLocation(WorldLocation);
	CachedDistance = Path->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

float UPurePursuitFollowerComponent::SampleUpcomingCurvature() const
{
	if (!Path.IsValid())
	{
		return 0.f;
	}

	const float SplineLength = Path->GetSplineLength();
	const bool bLoop = Path->IsClosedLoop();
	const int32 Samples = 4;
	const float Step = CurvatureProbeAhead / Samples;

	float WorstCurvature = 0.f;

	for (int32 i = 0; i < Samples; ++i)
	{
		const float DistA = CachedDistance + Step * i;
		const float DistB = DistA + Step;

		const float A = bLoop ? FMath::Fmod(DistA, SplineLength) : FMath::Min(DistA, SplineLength);
		const float B = bLoop ? FMath::Fmod(DistB, SplineLength) : FMath::Min(DistB, SplineLength);

		const FVector DirA = Path->GetDirectionAtDistanceAlongSpline(A, ESplineCoordinateSpace::World);
		const FVector DirB = Path->GetDirectionAtDistanceAlongSpline(B, ESplineCoordinateSpace::World);

		const float CosAngle = FMath::Clamp(FVector::DotProduct(DirA.GetSafeNormal2D(), DirB.GetSafeNormal2D()), -1.f, 1.f);
		const float AngleRad = FMath::Acos(CosAngle);

		// curvature = d(theta) / d(arc length)
		WorstCurvature = FMath::Max(WorstCurvature, AngleRad / FMath::Max(Step, 1.f));
	}

	return WorstCurvature;
}

float UPurePursuitFollowerComponent::ComputeAvoidanceScale() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return 1.f;
	}

	// Start high enough that the sphere clears the road surface.
	const FVector Start = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 250.f + FVector(0.f, 0.f, 100.f);
	const FVector End = Start + Owner->GetActorForwardVector() * ProbeDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VehicleAvoidance), false, Owner);
	FHitResult Hit;

	const bool bHit = World->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Vehicle,
		FCollisionShape::MakeSphere(ProbeRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugLine(World, Start, bHit ? Hit.Location : End, bHit ? FColor::Orange : FColor::Silver, false, -1.f, 0, 2.f);
	}

	// A near-vertical impact normal means we clipped the ground, not an obstacle.
	if (!bHit || Hit.ImpactNormal.Z > 0.7f)
	{
		return 1.f;
	}

	// Linear fade: full stop at contact, full speed at probe range.
	const float Ratio = FMath::Clamp(Hit.Distance / ProbeDistance, 0.f, 1.f);
	return FMath::Clamp(Ratio * Ratio, 0.f, 1.f);
}
