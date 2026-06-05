#include "StudentPerceptorThymmeMouchrique.h"

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

	bool IsRunnerZombieActor(const AActor* Actor)
	{
		return Actor && Actor->GetName().Contains(TEXT("Runner"));
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

	bool IsWeaponItemType(EItemType ItemType)
	{
		return ItemType == EItemType::Pistol ||
			ItemType == EItemType::Shotgun;
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

	bool InventoryHasWeapon(const UInventoryComponent* Inventory)
	{
		if (!Inventory)
		{
			return false;
		}

		return InventoryHasItemOfType(Inventory, EItemType::Pistol) ||
			InventoryHasItemOfType(Inventory, EItemType::Shotgun);
	}

	void SetBasicEnemyDecisionBlackboard(UBlackboardComponent* Blackboard, const UInventoryComponent* Inventory)
	{
		if (!Blackboard)
		{
			return;
		}

		const bool bHasWeapon = InventoryHasWeapon(Inventory);
		const bool bLowHealth = Blackboard->GetValueAsBool(TEXT("IsLowHealth"));

		Blackboard->SetValueAsBool(TEXT("ShouldAttackEnemy"), bHasWeapon);
		Blackboard->SetValueAsBool(TEXT("ShouldFleeEnemy"), !bHasWeapon || bLowHealth);
	}

	void UpdateEnemyDecisionBlackboard(
		UBlackboardComponent* Blackboard,
		APawn* OwnerPawn,
		const AActor* Enemy,
		const UInventoryComponent* Inventory)
	{
		if (!Blackboard || !OwnerPawn || !Enemy)
		{
			return;
		}

		const bool bIsRunner = IsRunnerZombieActor(Enemy);
		const bool bHasWeapon = InventoryHasWeapon(Inventory);
		const bool bLowHealth = Blackboard->GetValueAsBool(TEXT("IsLowHealth"));
		const bool bHasTargetHouse = Blackboard->GetValueAsObject(TEXT("TargetHouse")) != nullptr;

		bool bShouldFleeToHouse = false;
		bool bShouldAttackEnemy = false;
		bool bShouldFleeEnemy = false;

		if (bIsRunner)
		{
			// Runner is faster than the player
			// Best answer: hide in a house if we know one
			if (bHasTargetHouse)
			{
				bShouldFleeToHouse = true;
			}
			else if (bHasWeapon)
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
			// Normal/heavy zombies are less urgent
			if (bHasWeapon && !bLowHealth)
			{
				bShouldAttackEnemy = true;
			}
			else
			{
				bShouldFleeEnemy = true;
			}
		}

		Blackboard->SetValueAsBool(TEXT("IsRunnerEnemy"), bIsRunner);
		Blackboard->SetValueAsBool(TEXT("ShouldFleeToHouse"), bShouldFleeToHouse);
		Blackboard->SetValueAsBool(TEXT("ShouldAttackEnemy"), bShouldAttackEnemy);
		Blackboard->SetValueAsBool(TEXT("ShouldFleeEnemy"), bShouldFleeEnemy);

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

	int GetFreeSlotCount(const UInventoryComponent* Inventory)
	{
		if (!Inventory)
		{
			return 0;
		}

		int FreeSlots = 0;

		for (const ABaseItem* Item : Inventory->GetInventory())
		{
			if (!Item)
			{
				++FreeSlots;
			}
		}

		return FreeSlots;
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

		const bool bLowHealth = Blackboard->GetValueAsBool(TEXT("IsLowHealth"));
		const bool bLowEnergy = Blackboard->GetValueAsBool(TEXT("IsLowEnergy"));

		const bool bHasPistol = InventoryHasItemOfType(Inventory, EItemType::Pistol);
		const bool bHasShotgun = InventoryHasItemOfType(Inventory, EItemType::Shotgun);
		const bool bHasMedkit = InventoryHasItemOfType(Inventory, EItemType::Medkit);
		const bool bHasFood = InventoryHasItemOfType(Inventory, EItemType::Food);

		// "basic kit" just means having at least one of each type of item
		const bool bHasBasicKit = bHasPistol && bHasShotgun && bHasMedkit && bHasFood;

		const int FreeSlots = GetFreeSlotCount(Inventory);
		const bool bItemIsDuplicate =
			(ItemType == EItemType::Pistol && bHasPistol) ||
			(ItemType == EItemType::Shotgun && bHasShotgun) ||
			(ItemType == EItemType::Medkit && bHasMedkit) ||
			(ItemType == EItemType::Food && bHasFood);

		// prevent player gathering duplicate objects if it does not have 1 of each first
		if (!bHasBasicKit && bItemIsDuplicate)
		{
			return 0;
		}

		// pistol = 120
		// shotgun = 100
		// medkit = 90
		// food = 80

		// extra medkit if low HP = 70
		// extra food if low stamina = 60
		// extra weapon duplicate = 20

		if (ItemType == EItemType::Pistol)
		{
			return bHasPistol ? 20 : 120;
		}

		if (ItemType == EItemType::Shotgun)
		{
			return bHasShotgun ? 20 : 100;
		}

		if (ItemType == EItemType::Medkit)
		{
			if (!bHasMedkit)
			{
				return 90;
			}

			return bLowHealth ? 70 : 0;
		}

		if (ItemType == EItemType::Food)
		{
			if (!bHasFood)
			{
				return 80;
			}

			return bLowEnergy ? 60 : 0;
		}

		return 0;
	}

	float GetItemScore(const AActor* ItemActor, const UBlackboardComponent* Blackboard, const UInventoryComponent* Inventory, const FVector& FromLocation)
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

		// final score, higher score = better target
		return Priority - DistancePenalty;
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

	UHealthComponent* Health = OwnerPawn->FindComponentByClass<UHealthComponent>();

	if (!Health)
	{
		return;
	}

	// hack fix for not finding anything
	// do a 360 scan on spawn to find houses on spawn
	if (!bDidInitialScan)
	{
		UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] initiating spawn scan."));

		InitialScanTimer += DeltaTime;

		constexpr float ScanDuration = 2.0f;
		constexpr float ScanSpeedDegreesPerSecond = 180.f;

		OwnerPawn->AddActorWorldRotation(FRotator(0.f, ScanSpeedDegreesPerSecond * DeltaTime, 0.f));

		if (InitialScanTimer >= ScanDuration)
		{
			bDidInitialScan = true;
			UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] Finished initial spawn scan."));

			AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());
			UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;

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
		return;
	}

	const bool bTookDamage = CurrentHealth < LastHealth;
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (bTookDamage && CurrentTime - LastDamageReactionTime > 0.75f)
	{
		AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());
		UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;

		const bool bAlreadyHasEnemy = Blackboard && Blackboard->GetValueAsObject(TEXT("TargetEnemy")) != nullptr;

		if (!bAlreadyHasEnemy)
		{
			UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] Health dropped. No enemy known, turning around to scan."));

			OwnerPawn->AddActorWorldRotation(FRotator(0.f, 180.f, 0.f));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[STUDENTPERCEPTOR] Health dropped, but enemy already known. Not turning around."));
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

	LastVillageLocation = House->GetActorLocation();
	bHasLastVillageLocation = true;
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
	if (!bHasLastVillageLocation)
	{
		return false;
	}

	constexpr int MaxVillageExplorePoints = 8;

	if (VillageExploreIndex >= MaxVillageExplorePoints)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VillageExplore] Finished circle sweep."));
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

	// TODO: check radius 
	constexpr float CircleRadius = 1400.f;
	constexpr float ProjectionExtent = 600.f;

	const float AngleStep = 360.f / static_cast<float>(MaxVillageExplorePoints);
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

	if (!bProjected)
	{
		++VillageExploreIndex;
		return false;
	}

	OutLocation = ProjectedLocation.Location;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VillageExplore] Circle point %d/%d"),
		VillageExploreIndex + 1,
		MaxVillageExplorePoints
	);

	++VillageExploreIndex;
	return true;
}

void UStudentPerceptor::ResetVillageExploreIndex()
{
	VillageExploreIndex = 0;
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

	if (Health)
	{
		Blackboard->SetValueAsBool(TEXT("IsLowHealth"), Health->GetHealth() <= 5);
	}

	if (Stamina)
	{
		Blackboard->SetValueAsBool(TEXT("IsLowEnergy"), Stamina->GetCurrentStamina() <= 3.f);
	}

	if (Inventory)
	{
		Blackboard->SetValueAsBool(TEXT("HasWeapon"), InventoryHasWeapon(Inventory));
	}

	const bool bIsPurgeZone = IsPurgeZoneActor(Actor);
	const bool bIsZombie = IsZombieActor(Actor);
	const bool bIsHouse = IsHouseActor(Actor);
	const bool bIsUsefulItem = IsUsefulItemActor(Actor);

	if (bIsPurgeZone)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			UE_LOG(LogTemp, Warning, TEXT("SETTING TargetPurgeZone: %s"), *Actor->GetName());
			Blackboard->SetValueAsObject(TEXT("TargetPurgeZone"), Actor);
		}
		else
		{
			if (Blackboard->GetValueAsObject(TEXT("TargetPurgeZone")) == Actor)
			{
				UE_LOG(LogTemp, Warning, TEXT("CLEARING TargetPurgeZone: %s"), *Actor->GetName());
				Blackboard->ClearValue(TEXT("TargetPurgeZone"));
			}
		}

		return;
	}

	if (bIsZombie)
	{
		// read new enemy from perception but only update blackboard if new enemy is closer than current one
		if (Stimulus.WasSuccessfullySensed())
		{
			// before deciding what to do with a runner, try to make sure we have a known house target
			// if no known house exists, this does nothing
			TryUpdateTargetHouse();

			AActor* CurrentEnemy = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetEnemy")));

			if (!CurrentEnemy)
			{
				UE_LOG(LogTemp, Warning, TEXT("SETTING TargetEnemy: %s"), *Actor->GetName());
				Blackboard->SetValueAsObject(TEXT("TargetEnemy"), Actor);

				SetBasicEnemyDecisionBlackboard(Blackboard, Inventory);
				UpdateEnemyDecisionBlackboard(Blackboard, OwnerPawn, Actor, Inventory);
				return;
			}

			const float NewEnemyDistance = FVector::Dist2D(
				OwnerPawn->GetActorLocation(),
				Actor->GetActorLocation()
			);

			const float CurrentEnemyDistance = FVector::Dist2D(
				OwnerPawn->GetActorLocation(),
				CurrentEnemy->GetActorLocation()
			);

			if (NewEnemyDistance < CurrentEnemyDistance)
			{
				UE_LOG(LogTemp, Warning, TEXT("SWITCHING TargetEnemy to closer enemy: %s"), *Actor->GetName());
				Blackboard->SetValueAsObject(TEXT("TargetEnemy"), Actor);

				SetBasicEnemyDecisionBlackboard(Blackboard, Inventory);
				UpdateEnemyDecisionBlackboard(Blackboard, OwnerPawn, Actor, Inventory);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("KEEPING current TargetEnemy: %s"), *CurrentEnemy->GetName());

				SetBasicEnemyDecisionBlackboard(Blackboard, Inventory);
				UpdateEnemyDecisionBlackboard(Blackboard, OwnerPawn, CurrentEnemy, Inventory);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("LOST TargetEnemy, keeping memory"));
		}

		return;
	}

	if (bIsHouse)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			LastVillageLocation = Actor->GetActorLocation();
			bHasLastVillageLocation = true;

			if (!KnownHouses.Contains(Actor) && !SearchedHouses.Contains(Actor))
			{
				KnownHouses.Add(Actor);
				ResetVillageExploreIndex();
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

		// if inventory is full, there is no point in chasing the loot & clear TargetItem
		if (!InventoryHasFreeSlot(Inventory))
		{
			Blackboard->ClearValue(TEXT("TargetItem"));
			UE_LOG(LogTemp, Warning, TEXT("Inventory full, ignoring item: %s"), *Actor->GetName());
			return;
		}

		AActor* CurrentTargetItem = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetItem")));

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
			Blackboard->SetValueAsObject(TEXT("TargetItem"), Actor);
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

AActor* UStudentPerceptor::GetClosestKnownUnsearchedHouse(const FVector& FromLocation) const
{
	// returns closest unsearched house from known houses, but needs to be spotted to be added into known houses
	AActor* ClosestHouse = nullptr;
	float ClosestDistanceSq = TNumericLimits<float>::Max();

	for (AActor* House : KnownHouses)
	{
		if (!House || SearchedHouses.Contains(House))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(FromLocation, House->GetActorLocation());

		if (DistanceSq < ClosestDistanceSq)
		{
			ClosestDistanceSq = DistanceSq;
			ClosestHouse = House;
		}
	}

	return ClosestHouse;
}