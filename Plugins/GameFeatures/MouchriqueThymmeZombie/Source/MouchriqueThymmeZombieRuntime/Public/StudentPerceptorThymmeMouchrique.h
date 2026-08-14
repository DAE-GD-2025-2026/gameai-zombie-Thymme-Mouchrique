#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "StudentPerceptorThymmeMouchrique.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UStudentPerceptor : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStudentPerceptor();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// keep track of searched and unsearched houses in the area
	//TODO: private or public? 
	void MarkHouseSearched(AActor* House);
	AActor* GetClosestKnownUnsearchedHouse(const FVector& FromLocation) const;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	bool TryUpdateTargetHouse();
	bool TryUpdateKnownWeaponTarget();

	bool GetVillageExploreLocation(FVector& OutLocation) const;
	bool GetVillageCircleExploreLocation(FVector& OutLocation);
	void ResetVillageExploreIndex();

	// true once no houses/guns are known left in the village AND we're out of guns ourselves
	bool IsVillageDepleted() const;

	// call when an item is actually picked up so it stops counting as known loot
	void MarkItemCollected(AActor* Item);

	// check if enemy is actually visible right now and not just remembered
	bool IsEnemyCurrentlyVisible(const AActor* Enemy) const;

	// get last position where perception actually saw the enemy
	bool GetLastKnownEnemyLocation(const AActor* Enemy, FVector& OutLocation) const;

	// get all enemies that are actually visible right now for flee separation
	void GetVisibleEnemyLocations(TArray<FVector>& OutLocations) const;

	// purge task uses remembered position instead of reading hidden actor position
	bool GetLastKnownPurgeLocation(FVector& OutLocation) const;

	// true if location is still too close to somewhere we just fled from
	bool IsInRecentDangerZone(const FVector& Location) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<AActor>> KnownHouses;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SearchedHouses;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> KnownVillageWeapons;

	// hack fix for player getting hit by an enemy up their butt but not sensing it
	int LastHealth = -1;
	float LastDamageReactionTime = -1000.f;

	// update own health stamina and weapon state without needing a perception event
	float SelfStateAccumulator = 0.f;

	// remember where enemies were actually seen instead of tracking them through walls
	TMap<TWeakObjectPtr<AActor>, FVector> LastKnownEnemyLocations;
	TMap<TWeakObjectPtr<AActor>, float> LastEnemySeenTimes;
	TSet<TWeakObjectPtr<AActor>> VisibleEnemies;

	// remember purge location for a bit after losing sight so player does not instantly forget danger
	FVector LastKnownPurgeLocation = FVector::ZeroVector;
	float LastPurgeSeenTime = -1000.f;
	bool bHasKnownPurgeLocation = false;

	// where we last forgot about a threat, so explore does not walk straight back into it
	FVector LastDangerLocation = FVector::ZeroVector;
	float LastDangerTime = -1000.f;
	bool bHasDangerLocation = false;

	bool bDidInitialScan = false;
	float InitialScanTimer = 0.f;

	// village 
	FVector LastVillageLocation = FVector::ZeroVector;
	bool bHasLastVillageLocation = false;
	int VillageExploreIndex = 0;
	bool bVillageSweepConfirmedEmpty = false;

	bool HasVillageCampingResources() const;

	float SurvivalTime = 0.f;
	bool bLoggedDeath = false;
};
