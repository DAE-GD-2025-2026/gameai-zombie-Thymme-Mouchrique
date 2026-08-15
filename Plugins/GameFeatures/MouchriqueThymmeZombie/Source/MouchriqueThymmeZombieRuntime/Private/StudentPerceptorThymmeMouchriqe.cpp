#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

#include "NavigationSystem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"

#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
#include "Common/InventoryComponent.h"

#include "Items/BaseItem.h"
#include "Items/ItemType.h"
#include "Zombies/BaseZombie.h"
#include "Village/House/House.h"
#include "PurgeZones/PurgeZone.h"

namespace
{
	bool IsZombieActor(const AActor* Actor)
	{
		return Actor && Cast<ABaseZombie>(Actor) != nullptr;
	}

	bool IsHouseActor(const AActor* Actor)
	{
		return Actor && Cast<AHouse>(Actor) != nullptr;
	}

	bool IsPurgeZoneActor(const AActor* Actor)
	{
		return Actor && Cast<APurgeZone>(Actor) != nullptr;
	}

	bool IsUsefulItemActor(const AActor* Actor)
	{
		const ABaseItem* Item = Cast<ABaseItem>(Actor);

		if (!Item)
		{
			return false;
		}

		switch (Item->GetItemType())
		{
		case EItemType::Medkit:
		case EItemType::Food:
		case EItemType::Pistol:
		case EItemType::Shotgun:
			return true;

		default:
			return false;
		}
	}

	bool InventoryHasItemOfType(const UInventoryComponent* Inventory, EItemType WantedItemType)
	{
		if (!Inventory)
		{
			return false;
		}

		const TArray<ABaseItem*>& Items = Inventory->GetInventory();

		for (const ABaseItem* Item : Items)
		{
			if (Item && Item->GetItemType() == WantedItemType)
			{
				return true;
			}
		}

		return false;
	}

	// one place decides if player should attack or flee so decisions do not overwrite each other
	void UpdateCombatDecision(
		UBlackboardComponent* Blackboard,
		const EKnownZombieType EnemyType,
		const float EnemyDistance,
		const UInventoryComponent* Inventory)
	{
		if (!Blackboard)
		{
			return;
		}

		const bool bHasWeapon = InventoryHasUsableWeapon(Inventory);
		const bool bLowHealth = Blackboard->GetValueAsBool(SurvivorBB::IsLowHealth);

		bool bShouldFleeToHouse = false;
		bool bShouldAttackEnemy = false;
		bool bShouldFleeEnemy = false;

		const bool bIsRunner = EnemyType == EKnownZombieType::Runner;
		const bool bIsHeavy = EnemyType == EKnownZombieType::Heavy;

		if (bIsRunner)
		{
			// Runner is faster than the player
			if (bHasWeapon && !bLowHealth)
			{
				bShouldAttackEnemy = true;
			}
			else
			{
				bShouldFleeEnemy = true;
			}
		}
		else if (bIsHeavy)
		{
			// heavy hits hard so use the gun if we still have one instead of wasting time running
			if (bHasWeapon && !bLowHealth)
			{
				bShouldAttackEnemy = true;
			}
			else
			{
				bShouldFleeEnemy = true;
			}
		}
		else
		{
			// normal zombie is worth fighting while we still have ammo
			if (bHasWeapon && !bLowHealth)
			{
				bShouldAttackEnemy = true;
			}
			else
			{
				bShouldFleeEnemy = true;
			}
		}

		Blackboard->SetValueAsBool(SurvivorBB::IsRunnerEnemy, bIsRunner);
		Blackboard->SetValueAsBool(SurvivorBB::ShouldFleeToHouse, bShouldFleeToHouse);
		Blackboard->SetValueAsBool(SurvivorBB::ShouldAttackEnemy, bShouldAttackEnemy);
		Blackboard->SetValueAsBool(SurvivorBB::ShouldFleeEnemy, bShouldFleeEnemy);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[EnemyDecision] Runner=%s | FleeToHouse=%s | Attack=%s | Flee=%s"),
			bIsRunner ? TEXT("true") : TEXT("false"),
			bShouldFleeToHouse ? TEXT("true") : TEXT("false"),
			bShouldAttackEnemy ? TEXT("true") : TEXT("false"),
			bShouldFleeEnemy ? TEXT("true") : TEXT("false")
		);
	}

	bool InventoryHasFreeSlot(const UInventoryComponent* Inventory)
	{
		if (!Inventory)
		{
			return false;
		}

		const TArray<ABaseItem*>& Items = Inventory->GetInventory();

		for (const ABaseItem* Item : Items)
		{
			if (!Item)
			{
				return true;
			}
		}

		return false;
	}

	int GetItemPriority(const AActor* ItemActor, const UBlackboardComponent* Blackboard, const UInventoryComponent* Inventory)
	{
		if (!ItemActor || !Blackboard || !Inventory)
		{
			return 0;
		}

		const ABaseItem* Item = Cast<ABaseItem>(ItemActor);

		if (!Item)
		{
			return 0;
		}

		const EItemType ItemType = Item->GetItemType();

		const bool bLowHealth = Blackboard->GetValueAsBool(SurvivorBB::IsLowHealth);
		const bool bLowEnergy = Blackboard->GetValueAsBool(SurvivorBB::IsLowEnergy);

		const bool bHasPistol = InventoryHasItemOfType(Inventory, EItemType::Pistol);
		const bool bHasShotgun = InventoryHasItemOfType(Inventory, EItemType::Shotgun);
		const bool bHasMedkit = InventoryHasItemOfType(Inventory, EItemType::Medkit);
		const bool bHasFood = InventoryHasItemOfType(Inventory, EItemType::Food);

		const bool bHasUsableWeapon = InventoryHasUsableWeapon(Inventory);

		// critical stuff should come before normal looting
		if (ItemType == EItemType::Medkit && bLowHealth)
		{
			return 200;
		}

		if (ItemType == EItemType::Food && bLowEnergy)
		{
			return 180;
		}

		if (IsWeaponItemType(ItemType) && !bHasUsableWeapon && Item->GetValue() > 0)
		{
			return 170;
		}

		// extra guns are still useful because every gun is more ammo later
		if (ItemType == EItemType::Pistol)
		{
			return bHasPistol ? 80 : 130;
		}

		if (ItemType == EItemType::Shotgun)
		{
			return bHasShotgun ? 70 : 120;
		}

		if (ItemType == EItemType::Medkit)
		{
			return bHasMedkit ? 35 : 100;
		}

		if (ItemType == EItemType::Food)
		{
			return bHasFood ? 15 : 80;
		}

		return 0;
	}

	float GetItemScore(
		const AActor* ItemActor,
		const UBlackboardComponent* Blackboard,
		const UInventoryComponent* Inventory,
		const FVector& FromLocation)
	{
		if (!ItemActor)
		{
			return 0.f;
		}

		// get base usefulness (comes before priority rules)
		const int Priority = GetItemPriority(ItemActor, Blackboard, Inventory);

		// priority 0 means item is not useful right now
		// ex: duplicate item before the basic kit is complete
		if (Priority <= 0)
		{
			return 0.f;
		}

		// very far item should not always beat a nearby useful item

		const float Distance = FVector::Dist2D(FromLocation, ItemActor->GetActorLocation());
		const float DistancePenalty = Distance / 100.f;

		const ABaseItem* Item = Cast<ABaseItem>(ItemActor);
		float ValueBonus = 0.f;

		if (Item)
		{
			// use item value too because it contains ammo healing or stamina amount
			switch (Item->GetItemType())
			{
			case EItemType::Pistol:
			case EItemType::Shotgun:
				ValueBonus = Item->GetValue() * 3.f;
				break;

			case EItemType::Medkit:
				ValueBonus = Item->GetValue() * 4.f;
				break;

			case EItemType::Food:
				ValueBonus = Item->GetValue() * 2.f;
				break;

			default:
				break;
			}
		}

		// final score, higher score = better target
		return Priority + ValueBonus - DistancePenalty;
	}
}

UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();

	UAIPerceptionComponent* PerceptionComp = GetOwner()->FindComponentByClass<UAIPerceptionComponent>();

	if (!PerceptionComp)
	{
		return;
	}

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (UHealthComponent* Health = OwnerPawn->FindComponentByClass<UHealthComponent>())
		{
			LastHealth = Health->GetHealth();
		}
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);
}

void UStudentPerceptor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn)
	{
		return;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());
	UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// forget purge after a few seconds instead of tracking actor forever
	if (bHasKnownPurgeLocation)
	{
		const float DistanceToPurge = FVector::Dist2D(
			OwnerPawn->GetActorLocation(),
			LastKnownPurgeLocation);

		if (DistanceToPurge > 150.f || CurrentTime - LastPurgeSeenTime > 4.f)
		{
			if (Blackboard)
			{
				Blackboard->ClearValue(SurvivorBB::TargetPurgeZone);
			}

			if (CurrentTime - LastPurgeSeenTime > 4.f)
			{
				bHasKnownPurgeLocation = false;
			}
		}
	}

	// forget enemy after we have not seen it for a while and actually got away from it
	if (Blackboard)
	{
		AActor* TargetEnemy = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy));

		if (TargetEnemy && !IsEnemyCurrentlyVisible(TargetEnemy))
		{
			const TWeakObjectPtr<AActor> EnemyKey(TargetEnemy);
			const float* LastSeenTime = LastEnemySeenTimes.Find(EnemyKey);
			const FVector* LastKnownLocation = LastKnownEnemyLocations.Find(EnemyKey);

			if (LastSeenTime && LastKnownLocation)
			{
				const float TimeSinceSeen = CurrentTime - *LastSeenTime;
				const float DistanceFromEnemy = FVector::Dist2D(OwnerPawn->GetActorLocation(), *LastKnownLocation);

				if (TimeSinceSeen > 2.0f && DistanceFromEnemy > 1000.f)
				{
					// remember roughly where we just escaped from so we don't explore straight back into it
					LastDangerLocation = *LastKnownLocation;
					LastDangerTime = CurrentTime;
					bHasDangerLocation = true;

					Blackboard->ClearValue(SurvivorBB::TargetEnemy);
					Blackboard->SetValueAsBool(SurvivorBB::ShouldAttackEnemy, false);
					Blackboard->SetValueAsBool(SurvivorBB::ShouldFleeEnemy, false);
					Blackboard->SetValueAsBool(SurvivorBB::ShouldFleeToHouse, false);

					LastKnownEnemyLocations.Remove(EnemyKey);
					LastEnemySeenTimes.Remove(EnemyKey);
					VisibleEnemies.Remove(EnemyKey);
				}
			}
		}
	}

	// update own stuff every little bit even if perception does not fire
	SelfStateAccumulator += DeltaTime;

	if (SelfStateAccumulator >= 0.35f)
	{
		SelfStateAccumulator = 0.f;

		UHealthComponent* SelfHealth = OwnerPawn->FindComponentByClass<UHealthComponent>();
		UStaminaComponent* Stamina = OwnerPawn->FindComponentByClass<UStaminaComponent>();
		UInventoryComponent* Inventory = OwnerPawn->FindComponentByClass<UInventoryComponent>();

		if (Blackboard)
		{
			if (SelfHealth && SelfHealth->GetMaxHealth() > 0)
			{
				const float HealthRatio = static_cast<float>(SelfHealth->GetHealth())
					/ static_cast<float>(SelfHealth->GetMaxHealth());

				Blackboard->SetValueAsBool(SurvivorBB::IsLowHealth, HealthRatio <= 0.35f);
			}

			if (Stamina && Stamina->GetMaxStamina() > 0.f)
			{
				const float StaminaRatio = Stamina->GetCurrentStamina() / Stamina->GetMaxStamina();
				Blackboard->SetValueAsBool(SurvivorBB::IsLowEnergy, StaminaRatio <= 0.30f);
			}

			Blackboard->SetValueAsBool(SurvivorBB::HasWeapon, InventoryHasUsableWeapon(Inventory));

			// if we ran out of ammo, check guns we already saw before wandering around again
			if (!InventoryHasUsableWeapon(Inventory))
			{
				TryUpdateKnownWeaponTarget();
			}

			// update enemy decision too because ammo can change without a new perception event
			AActor* TargetEnemy = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy));

			if (TargetEnemy && IsEnemyCurrentlyVisible(TargetEnemy))
			{
				FVector EnemyLocation;

				if (GetLastKnownEnemyLocation(TargetEnemy, EnemyLocation))
				{
					const float EnemyDistance = FVector::Dist2D(OwnerPawn->GetActorLocation(), EnemyLocation);
					UpdateCombatDecision(Blackboard, ClassifyZombie(TargetEnemy), EnemyDistance, Inventory);

					UE_LOG(LogTemp, Warning, TEXT("[SelfState] HasWeapon=%s | TargetEnemy=%s"),
						InventoryHasUsableWeapon(Inventory) ? TEXT("true") : TEXT("false"),
						*GetNameSafe(TargetEnemy));

					UE_LOG(LogTemp, Warning, TEXT("[SelfStateDecision] Attack=%s | Flee=%s | FleeToHouse=%s"),
						Blackboard->GetValueAsBool(SurvivorBB::ShouldAttackEnemy) ? TEXT("true") : TEXT("false"),
						Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy) ? TEXT("true") : TEXT("false"),
						Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeToHouse) ? TEXT("true") : TEXT("false"));
				}
			}
		}
	}

	UHealthComponent* Health = OwnerPawn->FindComponentByClass<UHealthComponent>();

	if (!Health)
	{
		return;
	}

	if (Health->GetHealth() > 0)
	{
		SurvivalTime += DeltaTime;
	}
	else if (!bLoggedDeath)
	{
		bLoggedDeath = true;

		UE_LOG(LogTemp, Warning, TEXT("Player survived %.2f seconds"), SurvivalTime);
	}

	// hack fix for not finding anything
	// do a 360 scan on spawn to find houses on spawn
	if (!bDidInitialScan)
	{
		if (InitialScanTimer <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] initiating spawn scan."));
		}

		InitialScanTimer += DeltaTime;

		constexpr float ScanDuration = 2.0f;
		constexpr float ScanSpeedDegreesPerSecond = 180.f;

		OwnerPawn->AddActorWorldRotation(FRotator(0.f, ScanSpeedDegreesPerSecond * DeltaTime, 0.f));

		if (InitialScanTimer >= ScanDuration)
		{
			bDidInitialScan = true;
			UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] Finished initial spawn scan."));

			if (Blackboard)
			{
				Blackboard->SetValueAsBool(TEXT("HasFinishedInitialScan"), true);
			}

			TryUpdateTargetHouse();
		}

		return;
	}

	const int CurrentHealth = Health->GetHealth();

	if (LastHealth < 0)
	{
		LastHealth = CurrentHealth;
	}

	const bool bTookDamage = CurrentHealth < LastHealth;

	if (bTookDamage && CurrentTime - LastDamageReactionTime > 0.75f)
	{
		AActor* TargetEnemy = Blackboard
			? Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy))
			: nullptr;

		const bool bCanSeeTargetEnemy = TargetEnemy && IsEnemyCurrentlyVisible(TargetEnemy);

		if (!bCanSeeTargetEnemy)
		{
			const bool bAlreadyFleeing = Blackboard && Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy);

			UInventoryComponent* Inventory = OwnerPawn->FindComponentByClass<UInventoryComponent>();

			if (!bAlreadyFleeing && InventoryHasUsableWeapon(Inventory))
			{
				UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] Health dropped. No visible enemy, turning around to scan."));

				AIController->StopMovement();
				OwnerPawn->AddActorWorldRotation(FRotator(0.f, 180.f, 0.f));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] Health dropped while fleeing, keep moving."));
			}
		}

		LastDamageReactionTime = CurrentTime;
	}

	LastHealth = CurrentHealth;
}

void UStudentPerceptor::MarkHouseSearched(AActor* House)
{
	if (!House)
	{
		return;
	}

	if (!SearchedHouses.Contains(House))
	{
		SearchedHouses.Add(House);
		UE_LOG(LogTemp, Warning, TEXT("Marked house as searched: %s"), *House->GetName());
	}

	// remove house coz it already been searched
	KnownHouses.Remove(House);

	FVector KnownHouseLocation;

	if (GetKnownHouseLocation(House, KnownHouseLocation))
	{
		LastVillageLocation = KnownHouseLocation;
		bHasLastVillageLocation = true;
	}

	ResetVillageExploreIndex();
	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn)
	{
		return;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());

	if (!AIController)
	{
		return;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();

	if (!Blackboard)
	{
		return;
	}

	if (Blackboard->GetValueAsObject(TEXT("TargetHouse")) == House)
	{
		Blackboard->ClearValue(TEXT("TargetHouse"));
	}

	if (!TryUpdateTargetHouse())
	{
		UE_LOG(LogTemp, Warning, TEXT("No known unsearched houses left. Village exploration should continue."));
	}
}

bool UStudentPerceptor::TryUpdateTargetHouse()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn)
	{
		return false;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());

	if (!AIController)
	{
		return false;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();

	if (!Blackboard)
	{
		return false;
	}

	AActor* CurrentTargetHouse = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetHouse")));

	if (CurrentTargetHouse && IsValid(CurrentTargetHouse) && !SearchedHouses.Contains(CurrentTargetHouse))
	{
		return true;
	}

	AActor* ClosestHouse = GetClosestKnownUnsearchedHouse(OwnerPawn->GetActorLocation());

	if (!ClosestHouse)
	{
		Blackboard->ClearValue(TEXT("TargetHouse"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("SETTING TargetHouse from known houses: %s"), *ClosestHouse->GetName());

	Blackboard->SetValueAsObject(TEXT("TargetHouse"), ClosestHouse);
	Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), 0);

	return true;
}
bool UStudentPerceptor::TryUpdateKnownWeaponTarget()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn)
	{
		return false;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());

	if (!AIController)
	{
		return false;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	UInventoryComponent* Inventory = OwnerPawn->FindComponentByClass<UInventoryComponent>();

	if (!Blackboard || !Inventory)
	{
		return false;
	}

	// already going for something
	if (Blackboard->GetValueAsObject(SurvivorBB::TargetItem))
	{
		return true;
	}

	AActor* BestWeapon = nullptr;
	float BestScore = 0.f;

	for (int Index = KnownVillageWeapons.Num() - 1; Index >= 0; --Index)
	{
		AActor* Weapon = KnownVillageWeapons[Index];

		if (!IsValid(Weapon))
		{
			KnownVillageWeapons.RemoveAt(Index);
			continue;
		}

		const float Score = GetItemScore(Weapon, Blackboard, Inventory, OwnerPawn->GetActorLocation());

		if (Score > BestScore)
		{
			BestScore = Score;
			BestWeapon = Weapon;
		}
	}

	if (!BestWeapon)
	{
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("SETTING remembered weapon TargetItem: %s | Score: %.2f"), *BestWeapon->GetName(), BestScore);
	Blackboard->SetValueAsObject(SurvivorBB::TargetItem, BestWeapon);

	return true;
}
bool UStudentPerceptor::GetVillageExploreLocation(FVector& OutLocation) const
{
	if (!bHasLastVillageLocation)
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);

	if (!NavSystem)
	{
		return false;
	}

	FNavLocation NavLocation;

	constexpr float VillageExploreRadius = 1800.f;

	const bool bFoundLocation = NavSystem->GetRandomReachablePointInRadius(
		LastVillageLocation,
		VillageExploreRadius,
		NavLocation
	);

	if (!bFoundLocation)
	{
		return false;
	}

	OutLocation = NavLocation.Location;
	return true;
}

bool UStudentPerceptor::GetVillageCircleExploreLocation(FVector& OutLocation)
{
	if (!bHasLastVillageLocation || bVillageSweepConfirmedEmpty)
	{
		return false;
	}

	constexpr int MaxVillageExplorePoints = 8;

	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World);

	if (!NavSystem)
	{
		return false;
	}

	// TODO: check radius 
	constexpr float CircleRadius = 700.f;
	constexpr float ProjectionExtent = 250.f;

	const float AngleStep = 360.f / static_cast<float>(MaxVillageExplorePoints);

	while (VillageExploreIndex < MaxVillageExplorePoints)
	{
		const float AngleDegrees = AngleStep * static_cast<float>(VillageExploreIndex);
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

		const FVector Offset(
			FMath::Cos(AngleRadians) * CircleRadius,
			FMath::Sin(AngleRadians) * CircleRadius,
			0.f
		);

		const FVector DesiredLocation = LastVillageLocation + Offset;

		FNavLocation ProjectedLocation;

		const bool bProjected = NavSystem->ProjectPointToNavigation(
			DesiredLocation,
			ProjectedLocation,
			FVector(ProjectionExtent, ProjectionExtent, ProjectionExtent)
		);

		const int CurrentPoint = VillageExploreIndex;
		++VillageExploreIndex;

		if (!bProjected || IsInRecentDangerZone(ProjectedLocation.Location))
		{
			continue;
		}

		OutLocation = ProjectedLocation.Location;

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[VillageExplore] Circle point %d/%d"),
			CurrentPoint + 1,
			MaxVillageExplorePoints
		);

		return true;
	}

	// keep camping while there are still guns to use or resources we already know about
	if (HasVillageCampingResources())
	{
		VillageExploreIndex = 0;

		FNavLocation FallbackLocation;

		for (int Attempt = 0; Attempt < 6; ++Attempt)
		{
			if (!NavSystem->GetRandomReachablePointInRadius(LastVillageLocation, 700.f, FallbackLocation))
			{
				continue;
			}

			if (IsInRecentDangerZone(FallbackLocation.Location))
			{
				continue;
			}

			OutLocation = FallbackLocation.Location;

			UE_LOG(LogTemp, Warning, TEXT("[VillageExplore] Circle blocked, using local fallback."));
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("[VillageExplore] No safe local wander point found."));
		return false;
	}

	bVillageSweepConfirmedEmpty = true;
	UE_LOG(LogTemp, Warning, TEXT("[VillageExplore] Village depleted, time to find another one."));
	return false;
}

void UStudentPerceptor::ResetVillageExploreIndex()
{
	VillageExploreIndex = 0;
	bVillageSweepConfirmedEmpty = false;
}

bool UStudentPerceptor::IsEnemyCurrentlyVisible(const AActor* Enemy) const
{
	if (!Enemy)
	{
		return false;
	}

	return VisibleEnemies.Contains(TWeakObjectPtr<AActor>(const_cast<AActor*>(Enemy)));
}

bool UStudentPerceptor::GetLastKnownEnemyLocation(const AActor* Enemy, FVector& OutLocation) const
{
	if (!Enemy)
	{
		return false;
	}

	const TWeakObjectPtr<AActor> EnemyKey(const_cast<AActor*>(Enemy));

	const FVector* KnownLocation = LastKnownEnemyLocations.Find(EnemyKey);

	if (!KnownLocation)
	{
		return false;
	}

	OutLocation = *KnownLocation;
	return true;
}

void UStudentPerceptor::GetVisibleEnemyLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();

	for (const TWeakObjectPtr<AActor>& Enemy : VisibleEnemies)
	{
		if (!Enemy.IsValid())
		{
			continue;
		}

		const FVector* KnownLocation = LastKnownEnemyLocations.Find(Enemy);

		if (KnownLocation)
		{
			OutLocations.Add(*KnownLocation);
		}
	}
}

bool UStudentPerceptor::GetLastKnownPurgeLocation(FVector& OutLocation) const
{
	if (!bHasKnownPurgeLocation)
	{
		return false;
	}

	OutLocation = LastKnownPurgeLocation;
	return true;
}

bool UStudentPerceptor::GetLastKnownVillageLocation(FVector& OutLocation) const
{
	if (!bHasLastVillageLocation)
	{
		return false;
	}

	OutLocation = LastVillageLocation;
	return true;
}

bool UStudentPerceptor::HasVillageCampingResources() const
{
	if (KnownHouses.Num() > 0)
	{
		return true;
	}

	for (const AActor* Weapon : KnownVillageWeapons)
	{
		if (IsValid(Weapon))
		{
			return true;
		}
	}

	// if we still have ammo from this village, keep camping and use it before moving on
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const UInventoryComponent* Inventory = OwnerPawn ? OwnerPawn->FindComponentByClass<UInventoryComponent>() : nullptr;

	return InventoryHasUsableWeapon(Inventory) || GetTotalUsableAmmo(Inventory) > 0;
}

bool UStudentPerceptor::IsVillageDepleted() const
{
	return bHasLastVillageLocation && bVillageSweepConfirmedEmpty && !HasVillageCampingResources();
}

void UStudentPerceptor::MarkItemCollected(AActor* Item)
{
	if (KnownVillageWeapons.Remove(Item) > 0)
	{
		ResetVillageExploreIndex();
	}
}

bool UStudentPerceptor::IsInRecentDangerZone(const FVector& Location) const
{
	if (!bHasDangerLocation)
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	// put a cooldown on it so that we don't avoid this place forever	
	constexpr float DangerCooldown = 6.f;
	constexpr float DangerRadius = 800.f;

	if (World->GetTimeSeconds() - LastDangerTime > DangerCooldown)
	{
		return false;
	}

	const float CandidateDistance = FVector::Dist2D(Location, LastDangerLocation);

	if (CandidateDistance < DangerRadius)
	{
		return true;
	}

	// once we escaped, don't instantly pick a point that walks back toward the same zombie
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (OwnerPawn)
	{
		const float CurrentDistance = FVector::Dist2D(OwnerPawn->GetActorLocation(), LastDangerLocation);

		if (CandidateDistance + 100.f < CurrentDistance)
		{
			return true;
		}
	}

	return false;
}


void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("OwnerPawn invalid"));
		return;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());

	if (!AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("AIController invalid"));
		return;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();

	if (!Blackboard)
	{
		UE_LOG(LogTemp, Error, TEXT("Blackboard invalid"));
		return;
	}

	UHealthComponent* Health = OwnerPawn->FindComponentByClass<UHealthComponent>();
	UStaminaComponent* Stamina = OwnerPawn->FindComponentByClass<UStaminaComponent>();
	UInventoryComponent* Inventory = OwnerPawn->FindComponentByClass<UInventoryComponent>();

	if (Health && Health->GetMaxHealth() > 0)
	{
		const float HealthRatio =
			static_cast<float>(Health->GetHealth()) /
			static_cast<float>(Health->GetMaxHealth());

		Blackboard->SetValueAsBool(
			SurvivorBB::IsLowHealth,
			HealthRatio <= 0.35f
		);
	}

	if (Stamina && Stamina->GetMaxStamina() > 0.f)
	{
		const float StaminaRatio =
			Stamina->GetCurrentStamina() /
			Stamina->GetMaxStamina();

		Blackboard->SetValueAsBool(
			SurvivorBB::IsLowEnergy,
			StaminaRatio <= 0.30f
		);
	}

	if (Inventory)
	{
		Blackboard->SetValueAsBool(
			SurvivorBB::HasWeapon,
			InventoryHasUsableWeapon(Inventory)
		);
	}

	const bool bIsPurgeZone = IsPurgeZoneActor(Actor);
	const bool bIsZombie = IsZombieActor(Actor);
	const bool bIsHouse = IsHouseActor(Actor);
	const bool bIsUsefulItem = IsUsefulItemActor(Actor);

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (bIsPurgeZone)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			LastKnownPurgeLocation = Stimulus.StimulusLocation;
			LastPurgeSeenTime = CurrentTime;
			bHasKnownPurgeLocation = true;

			const float DistanceToPurge = FVector::Dist2D(
				OwnerPawn->GetActorLocation(),
				LastKnownPurgeLocation
			);

			// purge is tiny, only panic if we are basically inside it
			if (DistanceToPurge <= 100.f)
			{
				UE_LOG(LogTemp, Warning, TEXT("SETTING TargetPurgeZone: %s"), *Actor->GetName());
				Blackboard->SetValueAsObject(SurvivorBB::TargetPurgeZone, Actor);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("LOST TargetPurgeZone, keeping memory: %s"), *Actor->GetName());
		}

		return;
	}

	if (bIsZombie)
	{
		const TWeakObjectPtr<AActor> EnemyKey(Actor);

		// read new enemy from perception but only update blackboard if new enemy is closer than current one
		if (Stimulus.WasSuccessfullySensed())
		{
			// remember only what perception actually told us about this enemy
			LastKnownEnemyLocations.Add(EnemyKey, Stimulus.StimulusLocation);
			LastEnemySeenTimes.Add(EnemyKey, CurrentTime);
			VisibleEnemies.Add(EnemyKey);

			// before deciding what to do with a runner, try to make sure we have a known house target
			// if no known house exists, this does nothing
			TryUpdateTargetHouse();

			AActor* CurrentEnemy = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy));

			if (!CurrentEnemy)
			{
				UE_LOG(LogTemp, Warning, TEXT("SETTING TargetEnemy: %s"), *Actor->GetName());
				Blackboard->SetValueAsObject(SurvivorBB::TargetEnemy, Actor);

				const float EnemyDistance = FVector::Dist2D(
					OwnerPawn->GetActorLocation(),
					Stimulus.StimulusLocation
				);

				UpdateCombatDecision(
					Blackboard,
					ClassifyZombie(Actor),
					EnemyDistance,
					Inventory
				);

				return;
			}

			FVector CurrentEnemyLocation = FVector::ZeroVector;

			const bool bHasCurrentEnemyLocation = GetLastKnownEnemyLocation(CurrentEnemy, CurrentEnemyLocation);

			const bool bCurrentEnemyVisible = IsEnemyCurrentlyVisible(CurrentEnemy);

			const float NewEnemyDistance = FVector::Dist2D(OwnerPawn->GetActorLocation(), Stimulus.StimulusLocation);

			const float CurrentEnemyDistance = bHasCurrentEnemyLocation ? FVector::Dist2D(OwnerPawn->GetActorLocation(),
				CurrentEnemyLocation)
				: TNumericLimits<float>::Max();

			// visible enemy should replace an old invisible target instead of comparing against stale distance
			if (!bCurrentEnemyVisible || NewEnemyDistance < CurrentEnemyDistance)
			{
				UE_LOG(LogTemp, Warning, TEXT("SWITCHING TargetEnemy to closer enemy: %s"), *Actor->GetName());
				Blackboard->SetValueAsObject(SurvivorBB::TargetEnemy, Actor);

				UpdateCombatDecision(
					Blackboard,
					ClassifyZombie(Actor),
					NewEnemyDistance,
					Inventory
				);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("KEEPING current TargetEnemy: %s"), *CurrentEnemy->GetName());

				UpdateCombatDecision(
					Blackboard,
					ClassifyZombie(CurrentEnemy),
					CurrentEnemyDistance,
					Inventory
				);
			}
		}
		else
		{
			// stimulus location on lost sight is still perception information so keep it as last known spot
			LastKnownEnemyLocations.Add(EnemyKey, Stimulus.StimulusLocation);
			VisibleEnemies.Remove(EnemyKey);

			UE_LOG(LogTemp, Warning, TEXT("LOST TargetEnemy, keeping memory"));

			if (Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy) == Actor)
			{
				// cannot attack something we cannot currently see
				Blackboard->SetValueAsBool(SurvivorBB::ShouldAttackEnemy, false);
				Blackboard->SetValueAsBool(SurvivorBB::ShouldFleeToHouse, false);

				// if we were already fleeing, keep fleeing until the normal memory timeout says we got away
				if (Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy))
				{
					Blackboard->SetValueAsBool(SurvivorBB::ShouldFleeEnemy, true);
				}
			}
		}

		return;
	}

	if (bIsHouse)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			// house location came from perception so tasks can safely use it later
			KnownHouseLocations.Add(Actor, Stimulus.StimulusLocation);

			const bool bWasTravelling = Blackboard->GetValueAsBool(SurvivorBB::IsTravellingBetweenVillages);

			LastVillageLocation = Stimulus.StimulusLocation;
			bHasLastVillageLocation = true;
			Blackboard->SetValueAsBool(SurvivorBB::IsTravellingBetweenVillages, false);

			if (bWasTravelling)
			{
				ResetVillageExploreIndex();
			}

			if (!KnownHouses.Contains(Actor) && !SearchedHouses.Contains(Actor))
			{
				KnownHouses.Add(Actor);
				UE_LOG(LogTemp, Warning, TEXT("Added known house: %s"), *Actor->GetName());
			}

			AActor* CurrentTargetHouse = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetHouse")));

			if (!CurrentTargetHouse || SearchedHouses.Contains(CurrentTargetHouse))
			{
				TryUpdateTargetHouse();
			}
		}

		return;
	}

	if (bIsUsefulItem)
	{
		if (!Stimulus.WasSuccessfullySensed())
		{
			return;
		}

		// remember weapons specifically 
		// medkits/food are grabbed opportunistically
		if (const ABaseItem* SeenItem = Cast<ABaseItem>(Actor))
		{
			if (IsWeaponItemType(SeenItem->GetItemType()) && !KnownVillageWeapons.Contains(Actor))
			{
				KnownVillageWeapons.Add(Actor);
				ResetVillageExploreIndex();
			}
		}

		// if we find another medkit while hurt, use the one we already have first
		ABaseItem* SeenItem = Cast<ABaseItem>(Actor);

		if (SeenItem &&
			SeenItem->GetItemType() == EItemType::Medkit &&
			Health &&
			Health->GetHealth() < Health->GetMaxHealth())
		{
			const TArray<ABaseItem*>& Items = Inventory->GetInventory();

			for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
			{
				ABaseItem* InventoryItem = Items[SlotIdx];

				if (!InventoryItem || InventoryItem->GetItemType() != EItemType::Medkit)
				{
					continue;
				}

				if (Inventory->UseItem(SlotIdx))
				{
					// same cleanup as the normal healing task
					if (Items.IsValidIndex(SlotIdx) && Items[SlotIdx] && Items[SlotIdx]->GetValue() <= 0)
					{
						Inventory->RemoveItem(SlotIdx);
					}

					UE_LOG(LogTemp, Warning, TEXT("Used carried medkit to make room for: %s"), *Actor->GetName());

					// pick it up
					Blackboard->SetValueAsObject(SurvivorBB::TargetItem, Actor);
				}

				break;
			}
		}

		// if inventory is full, there is no point in chasing the loot & clear TargetItem
		if (!InventoryHasFreeSlot(Inventory))
		{
			Blackboard->ClearValue(SurvivorBB::TargetItem);
			UE_LOG(LogTemp, Warning, TEXT("Inventory full, ignoring item: %s"), *Actor->GetName());
			return;
		}

		AActor* CurrentTargetItem = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetItem));

		// score newly perceived item
		// score = item priority - distance penalty
		// ex: if pistol is high priority, but a very far pistol can lose to a useful nearby item
		const float NewScore = GetItemScore(Actor, Blackboard, Inventory, OwnerPawn->GetActorLocation());

		// score the current target item 
		// if there is no current target item use -1 so any valid item can replace it
		const float CurrentScore = CurrentTargetItem
			? GetItemScore(CurrentTargetItem, Blackboard, Inventory, OwnerPawn->GetActorLocation())
			: -1.f;

		// <= 0 means item is useless right now
		// ex: duplicate item before basic kit is complete, garbage or unwanted dupe
		if (NewScore <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("IGNORING TargetItem: %s | Score <= 0"), *Actor->GetName());
			return;
		}

		// if there is no current item, take this one
		if (!CurrentTargetItem || NewScore > CurrentScore)
		{
			UE_LOG(LogTemp, Warning, TEXT("SETTING TargetItem: %s | Score: %.2f"), *Actor->GetName(), NewScore);
			Blackboard->SetValueAsObject(SurvivorBB::TargetItem, Actor);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("IGNORING TargetItem: %s | Score: %.2f <= %.2f"),
				*Actor->GetName(),
				NewScore,
				CurrentScore
			);
		}

		return;
	}
}

bool UStudentPerceptor::GetKnownHouseLocation(const AActor* House, FVector& OutLocation) const
{
	if (!House)
	{
		return false;
	}

	const FVector* KnownLocation = KnownHouseLocations.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(House)));

	if (!KnownLocation)
	{
		return false;
	}

	OutLocation = *KnownLocation;
	return true;
}

AActor* UStudentPerceptor::GetClosestKnownUnsearchedHouse(const FVector& FromLocation) const
{
	AActor* ClosestHouse = nullptr;
	float ClosestDistanceSq = TNumericLimits<float>::Max();

	for (AActor* House : KnownHouses)
	{
		if (!House || SearchedHouses.Contains(House))
		{
			continue;
		}

		FVector HouseLocation;

		if (!GetKnownHouseLocation(House, HouseLocation))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(FromLocation, HouseLocation);

		if (DistanceSq < ClosestDistanceSq)
		{
			ClosestDistanceSq = DistanceSq;
			ClosestHouse = House;
		}
	}

	return ClosestHouse;
}