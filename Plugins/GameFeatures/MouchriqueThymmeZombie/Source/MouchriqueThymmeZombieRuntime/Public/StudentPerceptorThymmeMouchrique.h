// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "StudentPerceptorThymmeMouchrique.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UStudentPerceptor : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStudentPerceptor();
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

	// keep track of searched and unsearched houses in the area
	//TODO: private or public? 
	void MarkHouseSearched(AActor* House);
	AActor* GetClosestKnownUnsearchedHouse(const FVector& FromLocation) const;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	bool TryUpdateTargetHouse();
	bool GetVillageExploreLocation(FVector& OutLocation) const;
	bool GetVillageCircleExploreLocation(FVector& OutLocation);
	void ResetVillageExploreIndex();

private:
	UPROPERTY()
	TArray<TObjectPtr<AActor>> KnownHouses;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SearchedHouses;

	// hack fix for player getting hit by an enemy up their butt but not sensing it
	int LastHealth = -1;
	float LastDamageReactionTime = -1000.f;

	bool bDidInitialScan = false;
	float InitialScanTimer = 0.f;

	// village 
	FVector LastVillageLocation = FVector::ZeroVector;
	bool bHasLastVillageLocation = false;
	int VillageExploreIndex = 0;
};
