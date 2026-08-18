// AITrafficVehicle.cpp

#include "AITrafficVehicle.h"

#include "PurePursuitFollowerComponent.h"
#include "CrosswalkYieldComponent.h"
#include "TrafficRegistrySubsystem.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"

AAITrafficVehicle::AAITrafficVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bReplicates = true;
	SetReplicateMovement(true);

	// Distant traffic does not need frequent updates. The manager tunes this per LOD.
	SetNetUpdateFrequency(20.f);
	SetMinNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(225000000.f); // 150m

	Follower = CreateDefaultSubobject<UPurePursuitFollowerComponent>(TEXT("Follower"));
	Yield = CreateDefaultSubobject<UCrosswalkYieldComponent>(TEXT("Yield"));

	ConfigureVehicleDefaults();
}

void AAITrafficVehicle::ConfigureVehicleDefaults()
{
	// Mechanical setup lifted from the template's sports car. Keeping it in C++
	// means every AI vehicle is identically configured without per-Blueprint work,
	// and a reparented Blueprint cannot silently lose it.
	// Torque and steering curves still live on the Blueprint asset.
	UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	if (!Movement)
	{
		return;
	}

	// ---- Chassis ----
	Movement->ChassisHeight = 144.0f;
	Movement->DragCoefficient = 0.31f;
	Movement->bLegacyWheelFrictionPosition = true;

	// ---- Engine ----
	Movement->EngineSetup.MaxTorque = 750.0f;
	Movement->EngineSetup.MaxRPM = 7000.0f;
	Movement->EngineSetup.EngineIdleRPM = 900.0f;
	Movement->EngineSetup.EngineBrakeEffect = 0.2f;
	Movement->EngineSetup.EngineRevUpMOI = 5.0f;
	Movement->EngineSetup.EngineRevDownRate = 600.0f;

	// ---- Transmission ----
	Movement->TransmissionSetup.bUseAutomaticGears = true;
	Movement->TransmissionSetup.bUseAutoReverse = true;
	Movement->TransmissionSetup.FinalRatio = 2.81f;
	Movement->TransmissionSetup.ChangeUpRPM = 6000.0f;
	Movement->TransmissionSetup.ChangeDownRPM = 2000.0f;
	Movement->TransmissionSetup.GearChangeTime = 0.2f;
	Movement->TransmissionSetup.TransmissionEfficiency = 0.9f;

	Movement->TransmissionSetup.ForwardGearRatios = { 4.25f, 2.52f, 1.66f, 1.22f, 1.0f };
	Movement->TransmissionSetup.ReverseGearRatios = { 4.04f };

	// ---- Steering ----
	Movement->SteeringSetup.SteeringType = ESteeringType::Ackermann;
	Movement->SteeringSetup.AngleRatio = 0.7f;
}

void AAITrafficVehicle::BeginPlay()
{
	Super::BeginPlay();
	SetSimLOD(EVehicleSimLOD::Physics);
}

void AAITrafficVehicle::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UTrafficRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UTrafficRegistrySubsystem>() : nullptr)
	{
		Registry->ForgetVehicle(this);
	}

	Super::EndPlay(Reason);
}

void AAITrafficVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAITrafficVehicle, ReplicatedIntent);
}

void AAITrafficVehicle::InitialiseOnLane(USplineComponent* InSpline, int32 InLaneIndex, float StartDistance)
{
	Lane = InSpline;
	LaneIndex = InLaneIndex;
	KinematicDistance = StartDistance;

	if (Follower)
	{
		Follower->SetPath(InSpline, StartDistance);
	}

	if (Yield)
	{
		Yield->SetLaneIndex(InLaneIndex);
	}

	const FVector Location = InSpline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World);
	const FRotator Rotation = InSpline->GetRotationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World);
	SetActorLocationAndRotation(Location + FVector(0.f, 0.f, 60.f), FRotator(0.f, Rotation.Yaw, 0.f));
}

void AAITrafficVehicle::SetSimLOD(EVehicleSimLOD NewLOD)
{
	if (NewLOD == CurrentLOD)
	{
		return;
	}

	CurrentLOD = NewLOD;

	USkeletalMeshComponent* Body = GetMesh();
	UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());

	const bool bPhysics = (NewLOD == EVehicleSimLOD::Physics);

	if (Body)
	{
		Body->SetSimulatePhysics(bPhysics);
		Body->SetCollisionEnabled(bPhysics ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);
	}

	if (Movement)
	{
		Movement->SetComponentTickEnabled(bPhysics);
	}

	if (Follower)
	{
		Follower->SetComponentTickEnabled(bPhysics);
	}

	if (bPhysics)
	{
		// Hand the physics sim the state the kinematic sim was carrying.
		if (Follower && Lane)
		{
			Follower->SetPath(Lane, KinematicDistance);
		}
		if (Body && Lane)
		{
			const FVector Forward = Lane->GetDirectionAtDistanceAlongSpline(KinematicDistance, ESplineCoordinateSpace::World);
			Body->SetPhysicsLinearVelocity(Forward.GetSafeNormal() * KinematicSpeedCms);
		}
		SetReplicateMovement(true);
		SetNetUpdateFrequency(30.f);
	}
	else
	{
		// Take the physics sim's state into the kinematic sim so the swap is invisible.
		KinematicSpeedCms = GetVelocity().Size();
		if (Follower)
		{
			KinematicDistance = Follower->GetDistanceAlongPath();
		}
		SetReplicateMovement(!bReplicateIntentOnly);
		SetNetUpdateFrequency(bReplicateIntentOnly ? 2.f : 10.f);
	}
}

void AAITrafficVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentLOD == EVehicleSimLOD::Kinematic)
	{
		TickKinematic(DeltaTime);
	}

	if (HasAuthority() && bReplicateIntentOnly)
	{
		TimeSinceIntentPush += DeltaTime;
		if (TimeSinceIntentPush >= IntentUpdateInterval)
		{
			TimeSinceIntentPush = 0.f;
			PushIntent();
		}
	}
}

void AAITrafficVehicle::TickKinematic(float DeltaTime)
{
	if (!Lane)
	{
		return;
	}

	// Target speed comes from the same curvature rule the physics path uses,
	// so a vehicle does not visibly change behaviour when it swaps LOD.
	const float TargetSpeed = (Follower && Follower->GetTargetSpeedCms() > 0.f)
		? Follower->GetTargetSpeedCms()
		: Follower ? Follower->CruiseSpeedKph * 27.7778f : 1500.f;

	KinematicSpeedCms = FMath::FInterpTo(KinematicSpeedCms, TargetSpeed, DeltaTime, 1.5f);

	const float Length = Lane->GetSplineLength();
	KinematicDistance += KinematicSpeedCms * DeltaTime;
	if (Lane->IsClosedLoop())
	{
		KinematicDistance = FMath::Fmod(KinematicDistance, Length);
	}
	else
	{
		KinematicDistance = FMath::Min(KinematicDistance, Length);
	}

	const FVector Location = Lane->GetLocationAtDistanceAlongSpline(KinematicDistance, ESplineCoordinateSpace::World);
	const FRotator Rotation = Lane->GetRotationAtDistanceAlongSpline(KinematicDistance, ESplineCoordinateSpace::World);

	SetActorLocationAndRotation(Location + FVector(0.f, 0.f, 60.f), FRotator(0.f, Rotation.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void AAITrafficVehicle::PushIntent()
{
	ReplicatedIntent.LaneIndex = LaneIndex;
	ReplicatedIntent.DistanceAlongPath = (CurrentLOD == EVehicleSimLOD::Kinematic)
		? KinematicDistance
		: (Follower ? Follower->GetDistanceAlongPath() : 0.f);
	ReplicatedIntent.SpeedCms = (CurrentLOD == EVehicleSimLOD::Kinematic)
		? KinematicSpeedCms
		: GetVelocity().Size();
	ReplicatedIntent.ServerTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	ForceNetUpdate();
}

void AAITrafficVehicle::OnRep_Intent()
{
	if (HasAuthority())
	{
		return;
	}

	// Fast-forward by the one-way trip so the client lands where the server is now,
	// not where it was when the packet left.
	float Latency = 0.f;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			Latency = PS->GetPingInMilliseconds() * 0.001f * 0.5f;
		}
	}

	KinematicDistance = ReplicatedIntent.DistanceAlongPath + ReplicatedIntent.SpeedCms * Latency;
	KinematicSpeedCms = ReplicatedIntent.SpeedCms;
}