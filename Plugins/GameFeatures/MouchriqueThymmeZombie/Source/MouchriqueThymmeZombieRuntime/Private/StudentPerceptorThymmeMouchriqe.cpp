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
		if (!Actor)
		{
			return false;
		}

		return Actor->GetName().Contains(TEXT("House"));
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

	bool InventoryHasWeapon(const UInventoryComponent* Inventory)
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
				continue;
			}

			if (IsWeaponName(Item->GetName()))
			{
				return true;
			}
		}

		return false;
	}

	int GetItemPriority(const AActor* ItemActor, const UBlackboardComponent* Blackboard)
	{
		if (!ItemActor || !Blackboard)
		{
			return 0;
		}

		const FString Name = ItemActor->GetName();

		const bool bLowHealth = Blackboard->GetValueAsBool(TEXT("IsLowHealth"));
		const bool bLowEnergy = Blackboard->GetValueAsBool(TEXT("IsLowEnergy"));
		const bool bHasWeapon = Blackboard->GetValueAsBool(TEXT("HasWeapon"));

		if (Name.Contains(TEXT("Medkit")))
		{
			return bLowHealth ? 100 : 35;
		}

		if (IsWeaponName(Name))
		{
			return bHasWeapon ? 20 : 90;
		}

		if (Name.Contains(TEXT("Food")))
		{
			return bLowEnergy ? 80 : 40;
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
		const bool bLowHealth = Health->GetHealth() <= 3;
		Blackboard->SetValueAsBool(TEXT("IsLowHealth"), bLowHealth);
	}

	if (Stamina)
	{
		const bool bLowEnergy = Stamina->GetCurrentStamina() <= 3.f;
		Blackboard->SetValueAsBool(TEXT("IsLowEnergy"), bLowEnergy);
	}

	if (Inventory)
	{
		Blackboard->SetValueAsBool(TEXT("HasWeapon"), InventoryHasWeapon(Inventory));
	}

	const bool bIsZombie = IsZombieActor(Actor);
	const bool bIsHouse = IsHouseActor(Actor);
	const bool bIsUsefulItem = IsUsefulItemActor(Actor);

	if (bIsZombie)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			UE_LOG(LogTemp, Warning, TEXT("SETTING TargetEnemy: %s"), *Actor->GetName());
			Blackboard->SetValueAsObject(TEXT("TargetEnemy"), Actor);
		}
		else
		{
			// dont clear instantly or it will flicker too much
			UE_LOG(LogTemp, Warning, TEXT("LOST TargetEnemy, keeping memory"));
		}

		return;
	}

	if (bIsHouse)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			UE_LOG(LogTemp, Warning, TEXT("SETTING TargetHouse: %s"), *Actor->GetName());
			Blackboard->SetValueAsObject(TEXT("TargetHouse"), Actor);
		}

		return;
	}

	if (bIsUsefulItem)
	{
		if (!Stimulus.WasSuccessfullySensed())
		{
			return;
		}

		AActor* CurrentTargetItem = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetItem")));

		const int NewPriority = GetItemPriority(Actor, Blackboard);
		const int CurrentPriority = CurrentTargetItem ? GetItemPriority(CurrentTargetItem, Blackboard) : -1;

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