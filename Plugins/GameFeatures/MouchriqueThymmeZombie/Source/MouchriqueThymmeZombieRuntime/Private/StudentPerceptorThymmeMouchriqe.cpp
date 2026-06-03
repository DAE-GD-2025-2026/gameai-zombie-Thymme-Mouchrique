#include "StudentPerceptorThymmeMouchrique.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"

#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

namespace
{
	bool IsZombieActor(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		const FString Name = Actor->GetName();

		return Name.Contains(TEXT("Zombie")) ||
			Name.Contains(TEXT("Runner")) ||
			Name.Contains(TEXT("Heavy"));
	}

	bool IsHouseActor(const AActor* Actor)
	{
		return Actor && Actor->GetName().Contains(TEXT("House"));
	}

	bool IsPurgeZoneActor(const AActor* Actor)
	{
		return Actor && Actor->GetName().Contains(TEXT("Purge"));
	}

	bool IsUsefulItemActor(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		const FString Name = Actor->GetName();

		return Name.Contains(TEXT("Medkit")) ||
			Name.Contains(TEXT("Food")) ||
			Name.Contains(TEXT("Pistol")) ||
			Name.Contains(TEXT("Shotgun"));
	}

	bool IsWeaponName(const FString& Name)
	{
		return Name.Contains(TEXT("Pistol")) ||
			Name.Contains(TEXT("Shotgun"));
	}

	bool InventoryHasItemContainingName(const UInventoryComponent* Inventory, const FString& ItemName)
	{
		if (!Inventory)
		{
			return false;
		}

		const TArray<ABaseItem*>& Items = Inventory->GetInventory();

		for (const ABaseItem* Item : Items)
		{
			if (Item && Item->GetName().Contains(ItemName))
			{
				return true;
			}
		}

		return false;
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

	bool InventoryHasWeapon(const UInventoryComponent* Inventory)
	{
		if (!Inventory)
		{
			return false;
		}

		return InventoryHasItemContainingName(Inventory, TEXT("Pistol")) ||
			InventoryHasItemContainingName(Inventory, TEXT("Shotgun"));
	}

	int GetItemPriority(const AActor* ItemActor, const UBlackboardComponent* Blackboard, const UInventoryComponent* Inventory)
	{
		if (!ItemActor || !Blackboard || !Inventory)
		{
			return 0;
		}

		const FString Name = ItemActor->GetName();

		const bool bLowHealth = Blackboard->GetValueAsBool(TEXT("IsLowHealth"));
		const bool bLowEnergy = Blackboard->GetValueAsBool(TEXT("IsLowEnergy"));

		const bool bHasPistol = InventoryHasItemContainingName(Inventory, TEXT("Pistol"));
		const bool bHasShotgun = InventoryHasItemContainingName(Inventory, TEXT("Shotgun"));
		const bool bHasMedkit = InventoryHasItemContainingName(Inventory, TEXT("Medkit"));
		const bool bHasFood = InventoryHasItemContainingName(Inventory, TEXT("Food"));

		// 1. Pistol above everything
		if (Name.Contains(TEXT("Pistol")))
		{
			return bHasPistol ? 15 : 120;
		}

		// 2. Shotgun is still useful
		if (Name.Contains(TEXT("Shotgun")))
		{
			return bHasShotgun ? 10 : 100;
		}

		// 3. First medkit is important, extra medkits only matter when low health
		if (Name.Contains(TEXT("Medkit")))
		{
			if (!bHasMedkit)
			{
				return 90;
			}

			return bLowHealth ? 80 : 20;
		}

		// 4. First food is useful, extra food only matters when stamina is low
		if (Name.Contains(TEXT("Food")))
		{
			if (!bHasFood)
			{
				return 70;
			}

			return bLowEnergy ? 60 : 15;
		}

		return 0;
	}
}

UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("StudentPerceptor BeginPlay"));

	UAIPerceptionComponent* PerceptionComp = GetOwner()->FindComponentByClass<UAIPerceptionComponent>();

	if (!PerceptionComp)
	{
		UE_LOG(LogTemp, Error, TEXT("No PerceptionComponent found"));
		return;
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);

	UE_LOG(LogTemp, Warning, TEXT("Bound perception callback"));
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
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Perceived Actor: %s"), *Actor->GetName());

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
		Blackboard->SetValueAsBool(TEXT("IsLowHealth"), Health->GetHealth() <= 3);
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
		if (Stimulus.WasSuccessfullySensed())
		{
			UE_LOG(LogTemp, Warning, TEXT("SETTING TargetEnemy: %s"), *Actor->GetName());
			Blackboard->SetValueAsObject(TEXT("TargetEnemy"), Actor);
		}
		else
		{
			// don't clear TargetEnemy immediately or it will flicker
			//TODO: fix
			UE_LOG(LogTemp, Warning, TEXT("LOST TargetEnemy, keeping memory"));
		}

		return;
	}

	if (bIsHouse)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			if (SearchedHouses.Contains(Actor))
			{
				UE_LOG(LogTemp, Warning, TEXT("IGNORING searched house: %s"), *Actor->GetName());
				return;
			}

			AActor* CurrentTargetHouse = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetHouse")));

			if (!CurrentTargetHouse)
			{
				UE_LOG(LogTemp, Warning, TEXT("SETTING TargetHouse: %s"), *Actor->GetName());
				Blackboard->SetValueAsObject(TEXT("TargetHouse"), Actor);
				Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), 0);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("IGNORING House, already searching: %s"), *CurrentTargetHouse->GetName());
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

		if (!InventoryHasFreeSlot(Inventory))
		{
			Blackboard->ClearValue(TEXT("TargetItem"));
			UE_LOG(LogTemp, Warning, TEXT("Inventory full, ignoring item: %s"), *Actor->GetName());
			return;
		}

		AActor* CurrentTargetItem = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetItem")));

		const int NewPriority = GetItemPriority(Actor, Blackboard, Inventory);
		const int CurrentPriority = CurrentTargetItem ? GetItemPriority(CurrentTargetItem, Blackboard, Inventory) : -1;

		if (!CurrentTargetItem || NewPriority > CurrentPriority)
		{
			UE_LOG(LogTemp, Warning, TEXT("SETTING TargetItem: %s | Priority: %d"), *Actor->GetName(), NewPriority);
			Blackboard->SetValueAsObject(TEXT("TargetItem"), Actor);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("IGNORING TargetItem: %s | Priority: %d <= %d"), *Actor->GetName(), NewPriority, CurrentPriority);
		}

		return;
	}
}