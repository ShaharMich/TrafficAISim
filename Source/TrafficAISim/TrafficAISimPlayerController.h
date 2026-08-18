// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TrafficAISimPlayerController.generated.h"

class UInputMappingContext;
class ATrafficAISimPawn;
class UTrafficAISimUI;

/**
 *  Vehicle Player Controller class
 *  Handles input mapping and user interface
 */
UCLASS(abstract)
class ATrafficAISimPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the optional steering wheel input mapping context will be registered */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls")
	bool bUseSteeringWheelControls = false;

	/** Optional Input Mapping Context to be used for steering wheel input.
	 *  This is added alongside the default Input Mapping Context and does not block other forms of input.
	 */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls", meta = (EditCondition = "bUseSteeringWheelControls"))
	UInputMappingContext* SteeringWheelInputMappingContext;

	/** Type of vehicle to automatically respawn when it's destroyed */
	UPROPERTY(EditAnywhere, Category="Vehicle|Respawn")
	TSubclassOf<ATrafficAISimPawn> VehiclePawnClass;

	/** Pointer to the controlled vehicle pawn */
	TObjectPtr<ATrafficAISimPawn> VehiclePawn;

	/** Type of the UI to spawn */
	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<UTrafficAISimUI> VehicleUIClass;

	/** Pointer to the UI widget */
	TObjectPtr<UTrafficAISimUI> VehicleUI;
		
protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input setup */
	virtual void SetupInputComponent() override;

public:

	/** Update vehicle UI on tick */
	virtual void Tick(float Delta) override;

protected:

	/** Pawn setup */
	virtual void OnPossess(APawn* InPawn) override;

	/** Handles pawn destruction and respawning */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedPawn);
};
